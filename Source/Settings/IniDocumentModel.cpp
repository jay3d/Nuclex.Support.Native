#pragma region CPL License
/*
Nuclex Native Framework
Copyright (C) 2002-2021 Nuclex Development Labs

This library is free software; you can redistribute it and/or
modify it under the terms of the IBM Common Public License as
published by the IBM Corporation; either version 1.0 of the
License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
IBM Common Public License for more details.

You should have received a copy of the IBM Common Public
License along with this library
*/
#pragma endregion // CPL License

// If the library is compiled as a DLL, this ensures symbols are exported
#define NUCLEX_SUPPORT_SOURCE 1

#include "IniDocumentModel.h"

#include <memory> // for std::unique_ptr
#include <type_traits> // for std::is_base_of
#include <algorithm> // for std::copy_n()
#include <cassert> // for assert()

// Ambiguous cases and their resolution:
//
//   ["Hello]"       -> Malformed
//   [World          -> Malformed
//   [Foo] = Bar     -> Assignment, no section
//   [Woop][Woop]    -> Two sections, one w/newline one w/o
//   [Foo] Bar = Baz -> Section and assignment
//   [[Yay]          -> Malformed, section
//

// Allocation schemes:
//
//   By line                      -> lots of micro-allocations
//   In blocks (custom allocator) -> I have to do reference counting to free anything
//   Load pre-alloc, then by line -> Fast for typical case, no or few micro-allocations
//

namespace {

  // ------------------------------------------------------------------------------------------- //

  /// <summary>Grows the specified byte count until hittint the required alignment</summary>
  /// <param name="byteCount">Byte counter that will be grown until aligned</param>
  /// <param name="alignment">Alignment the byte counter must have</param>
  void growUntilAligned(std::size_t &byteCount, std::size_t alignment) {
    std::size_t extraByteCount = byteCount % alignment;
    if(extraByteCount > 0) {
      byteCount += (alignment - extraByteCount);
    }
  }

  // ------------------------------------------------------------------------------------------- //

  /// <summary>Checks whether the specified character is a whiteapce</summary>
  /// <param name="utf8SingleByteCharacter">
  ///   Character the will be checked for being a whitespace
  /// </param>
  /// <returns>True if the character was a whitespace, false otherwise</returns>
  bool isWhitepace(std::uint8_t utf8SingleByteCharacter) {
    return (
      (utf8SingleByteCharacter == ' ') ||
      (utf8SingleByteCharacter == '\t') ||
      (utf8SingleByteCharacter == '\r') ||
      (utf8SingleByteCharacter == '\n')
    );
  }

  // ------------------------------------------------------------------------------------------- //

  /// <summary>Skips whitespace before and after other characters</summary>
  /// <param name="begin">String beginning to trim in forward direction</param>
  /// <param name="end">One past the string's end to trim backwards</param>
  void trim(const std::uint8_t *begin, const std::uint8_t *end) {
    while(end > begin) {
      --end;
      if(!isWhitepace(*end)) {
        ++end;
        break;
      }
    }

    while(begin < end) {
      if(!isWhitepace(*begin)) {
        break;
      }

      ++begin;
    }
  }

  // ------------------------------------------------------------------------------------------- //

} // anonymous namespace

namespace Nuclex { namespace Support { namespace Settings {

  // ------------------------------------------------------------------------------------------- //

  /// <summary>
  ///   Accumulates an estimate of the memory required to hold the document model
  /// </summary>
  class Nuclex::Support::Settings::IniDocumentModel::MemoryEstimator {

    /// <summary>Initializes a new memory estimator</summary>
    public: MemoryEstimator() :
      ByteCount(0),
      lineStartPosition(nullptr),
      sectionStarted(false),
      sectionEnded(false),
      foundAssignment(false),
      lineIsMalformed(false) {}

    /// <summary>Notifies the memory estimator that a new line has begun</summary>
    /// <param name="filePosition">Current position in the file scan</param>
    public: void BeginLine(const std::uint8_t *filePosition) {
      if(this->lineStartPosition != nullptr) {
        EndLine(filePosition);
      }

      this->lineStartPosition = filePosition;
    }

    /// <summary>Notifies the memory estimator that the current line has ended</summary>
    /// <param name="filePosition">Current position in the file scan</param>
    public: void EndLine(const std::uint8_t *filePosition) {
      if(this->lineIsMalformed) {
        growUntilAligned(this->ByteCount, alignof(Line));
        this->ByteCount += sizeof(Line[2]) / 2;
      } else if(this->foundAssignment) {
        growUntilAligned(this->ByteCount, alignof(PropertyLine));
        this->ByteCount += sizeof(PropertyLine[2]) / 2;
      } else if(this->sectionStarted && this->sectionEnded) {
        growUntilAligned(this->ByteCount, alignof(SectionLine));
        this->ByteCount += sizeof(SectionLine[2]) / 2;
      } else {
        growUntilAligned(this->ByteCount, alignof(Line));
        this->ByteCount += sizeof(Line[2]) / 2;
      }

      this->ByteCount += (filePosition - this->lineStartPosition);
      this->lineStartPosition = filePosition;

      this->sectionStarted = false;
      this->sectionEnded = false;
      this->foundAssignment =false;
      this->lineIsMalformed = false;
    }

    /// <summary>Notifies the memory estimator that a section has been opened</summary>
    /// <param name="filePosition">Current position in the file scan</param>
    public: void BeginSection(const std::uint8_t *filePosition) {
      if(this->sectionEnded) {
        EndLine(filePosition);
      }

      this->sectionStarted = true;
    }

    /// <summary>Notifies the memory estimator that a section has been closed</summary>
    public: void EndSection() {
      if(this->sectionStarted) {
        this->sectionEnded = true;
      }
    }

    /// <summary>Notifies the memory estimator that an equals sign has been found</summary>
    public: void AddAssignment() {
      if(this->foundAssignment) {
        this->lineIsMalformed = true;
      } else {
        this->foundAssignment = true;
      }
    }

    /// <summary>Number of bytes accumulated so far</summary>
    public: std::size_t ByteCount;
    /// <summary>File offset at which the current line begins</summary>
    private: const std::uint8_t *lineStartPosition;
    /// <summary>Whether a section start marker was encountered</summary>
    private: bool sectionStarted;
    /// <summary>Whether a section end marker was encountered</summary>
    private: bool sectionEnded;
    /// <summary>Whether an equals sign was encountered</summary>
    private: bool foundAssignment;
    /// <summary>Whether we a proof that the current line is malformed</summary>
    private: bool lineIsMalformed;

  };

  // ------------------------------------------------------------------------------------------- //

  /// <summary>Builds the document model according to the parsed file contents</summary>
  class IniDocumentModel::ModelBuilder {

    /// <summary>Initializes a new model builder filling the specified document model</summary>
    /// <param name="targetDocumentModel">Document model the builder will fill</param>
    /// <param name="allocatedByteCount">
    ///   Number of bytes the document model has allocated as the initial chunk
    /// </param>
    public: ModelBuilder(IniDocumentModel *targetDocumentModel, std::size_t allocatedByteCount) :
      targetDocumentModel(targetDocumentModel),
      allocatedByteCount(allocatedByteCount),
      lineStartPosition(nullptr),
      sectionStartPosition(nullptr),
      sectionEndPosition(nullptr),
      equalsSignPosition(nullptr),
      isMalformedLine(false) {}

    /// <summary>Notifies the model builder that a new line has begun</summary>
    /// <param name="filePosition">Current position in the parsed file</param>
    public: void BeginLine(const std::uint8_t *filePosition) {
      if(this->lineStartPosition != nullptr) {
        EndLine(filePosition);
      }

      this->lineStartPosition = filePosition;
    }

    /// <summary>Notifies the memory estimator that the current line has ended</summary>
    /// <param name="filePosition">Current position in the file scan</param>
    public: void EndLine(const std::uint8_t *filePosition) {
      if(!this->isMalformedLine) {
        if(this->equalsSignPosition != nullptr) { // Is it a property assignment line?
          PropertyLine *newLine = addLine<PropertyLine>(this->lineStartPosition, filePosition);
        } else if(this->sectionEndPosition != nullptr) { // end is only set when start is set
          SectionLine *newLine = addLine<SectionLine>(this->lineStartPosition, filePosition);
          //newLine->NameStartIndex = nameStart - this->lineStartPosition;
          //newLine->NameLength = nameEnd - nameStart;
          //this->targetDocumentModel->sections.insert(
          //  new IndexedSection()
          //)
        } else { // It's a meaningless line
          Line *newLine = addLine<Line>(this->lineStartPosition, filePosition);
        }
      }

      this->lineStartPosition = filePosition;
      
      this->sectionStartPosition = nullptr;
      this->sectionEndPosition = nullptr;
      this->equalsSignPosition = nullptr;
      this->isMalformedLine = false;
    }

    /// <summary>Notifies the memory estimator that a section has been opened</summary>
    /// <param name="filePosition">Current position in the file scan</param>
    public: void BeginSection(const std::uint8_t *filePosition) {
      if(this->sectionEndPosition != nullptr) {

        // If there is a previous section, that means there are two sections in the same
        // line and we need to cut the current line in two
        if(this->sectionEndPosition == nullptr) {
          this->isMalformedLine = true;
        } else {
          EndLine(filePosition);
        }

      }

      this->sectionStartPosition = filePosition;
    }

    /// <summary>Notifies the memory estimator that a section has been closed</summary>
    /// <param name="filePosition">Current position in the file scan</param>
    /// <param name="nameStart">Position in the file where the section name begins</param>
    /// <param name="nameEnd">File position one after the end of the section name</param>
    public: void EndSection(
      const std::uint8_t *filePosition,
      const std::uint8_t *nameStart, const std::uint8_t *nameEnd
    ) {
      if(this->sectionStartPosition == nullptr) {
        this->isMalformedLine = true;
      } else if(this->sectionEndPosition == nullptr) {
        this->sectionEndPosition = filePosition;
      } else {
        this->isMalformedLine = true;
      }
    }

/*
    /// <summary>Notifies the memory estimator that an equals sign has been found</summary>
    public: void AddAssignment() {
      if(this->foundAssignment) {
        this->lineIsMalformed = true;
      } else {
        this->foundAssignment = true;
      }
    }
*/

    private: template<typename TLine>
    TLine *addLine(const std::uint8_t *lineBegin, const std::uint8_t *lineEnd) { 
      // initialize contents and length, too.
      // decrement allocatedByteCount
      // link new line into structures
      return nullptr;
    }

    /// <summary>Document model this builder will fill with parsed elements</summary>
    private: IniDocumentModel *targetDocumentModel;
    /// <summary>Remaining bytes available of the document model's pre-allocation</summary>
    private: std::size_t allocatedByteCount;

    /// <summary>File offset at which the current line begins</summary>
    private: const std::uint8_t *lineStartPosition;
    /// <summary>File offset at which the current section starts, if any</summary>
    private: const std::uint8_t *sectionStartPosition;
    /// <summary>File offset at which the current section ended, if any</summary>
    private: const std::uint8_t *sectionEndPosition;
    /// <summary>File offset at which the current assignment's equals sign is</summary>
    private: const std::uint8_t *equalsSignPosition;
    /// <summary>Do we have conclusive evidence that the line is malformed?</summary>
    private: bool isMalformedLine;

  };

  // ------------------------------------------------------------------------------------------- //

  IniDocumentModel::IniDocumentModel() :
    loadedLinesMemory(nullptr),
    createdLinesMemory(),
    firstLine(nullptr),
    sections(),
    hasSpacesAroundAssignment(true),
    hasEmptyLinesBetweenProperties(true) {

#if defined(NUCLEX_SUPPORT_WIN32)
    this->firstLine = allocateLine<Line>(reinterpret_cast<const std::uint8_t *>(u8"\r\n"), 2);
#else
    this->firstLine = allocateLine<Line>(reinterpret_cast<const std::uint8_t *>(u8"\n"), 1);
#endif
    this->firstLine->Previous = this->firstLine;
    this->firstLine->Next = this->firstLine;
  }

  // ------------------------------------------------------------------------------------------- //

  IniDocumentModel::IniDocumentModel(const std::uint8_t *fileContents, std::size_t byteCount) :
    loadedLinesMemory(nullptr),
    createdLinesMemory(),
    firstLine(nullptr),
    sections(),
    hasSpacesAroundAssignment(true),
    hasEmptyLinesBetweenProperties(true) {

    std::size_t requiredMemory = estimateRequiredMemory(fileContents, byteCount);
    this->loadedLinesMemory = new std::uint8_t[requiredMemory];
  }

  // ------------------------------------------------------------------------------------------- //

  IniDocumentModel::~IniDocumentModel() {

    // This can be a nullptr if a new .ini file was constructed, but deleting
    // a nullptr is allowed (it's a no-op) by the C++ standard
    delete[] this->loadedLinesMemory;

    // Delete the memory for any other lines that were created
    for(
      std::unordered_set<std::uint8_t *>::iterator iterator = this->createdLinesMemory.begin();
      iterator != this->createdLinesMemory.end();
      ++iterator
    ) {
      delete[] *iterator;
    }

  }

  // ------------------------------------------------------------------------------------------- //

  std::size_t IniDocumentModel::estimateRequiredMemory(
    const std::uint8_t *fileContents, std::size_t byteCount
  ) {
    MemoryEstimator memoryEstimate;
    {
      bool previousWasNewLine = false;
      bool isInsideQuote = false;
      bool encounteredComment = false;

      // Make sure the memory estimator knows where the first line starts
      memoryEstimate.BeginLine(fileContents);

      // Go through the entire file contents byte-by-byte and do some basic parsing
      // to handle quotes and comments correctly. All of these characters are in
      // the ASCII range, thus there are no UTF-8 sequences that could be mistaken for
      // them (multi-byte UTF-8 codepoints will have the highest bit set in all bytes)
      const std::uint8_t *fileBegin = fileContents;
      const std::uint8_t *fileEnd = fileContents + byteCount;
      while(fileBegin < fileEnd) {
        std::uint8_t current = *fileBegin;

        // Newlines always reset the state, this parser does not support multi-line statements
        previousWasNewLine = (current == '\n');
        if(previousWasNewLine) {
          isInsideQuote = false;
          encounteredComment = false;
          memoryEstimate.EndLine(fileBegin + 1);
        } else if(isInsideQuote) { // Quotes make it ignore section and assignment characters
          if(current == '"') {
            isInsideQuote = false;
          }
        } else if(!encounteredComment) { // Otherwise, parse until comment encountered
          switch(current) {
            case ';':
            case '#': { encounteredComment = true; break; }
            case '[': { memoryEstimate.BeginSection(fileBegin); break; }
            case ']': { memoryEstimate.EndSection(); break; }
            case '=': { memoryEstimate.AddAssignment(); break; }
            case '"': { isInsideQuote = true; break; }
            default: { break; }
          }
        }

        ++fileBegin;
      } // while fileBegin < fileEnd

      // If the file didn't end with a line break, consider the contents of
      // the file's final line as another line
      if(!previousWasNewLine) {
        memoryEstimate.EndLine(fileEnd);
      }
    } // beauty scope

    return memoryEstimate.ByteCount;
  }

  // ------------------------------------------------------------------------------------------- //

  void IniDocumentModel::parseFileContents(
    const std::uint8_t *fileContents, std::size_t byteCount,
    std::size_t allocatedByteCount
  ) {
    ModelBuilder modelBuilder(this, allocatedByteCount);
    {
      bool previousWasNewLine = false;
      bool isInsideQuote = false;
      bool encounteredComment = false;

      // Make sure the memory estimator knows where the first line starts
      modelBuilder.BeginLine(fileContents);

      // Go through the entire file contents byte-by-byte and do some basic parsing
      // to handle quotes and comments correctly. All of these characters are in
      // the ASCII range, thus there are no UTF-8 sequences that could be mistaken for
      // them (multi-byte UTF-8 codepoints will have the highest bit set in all bytes)
      const std::uint8_t *fileBegin = fileContents;
      const std::uint8_t *fileEnd = fileContents + byteCount;
      while(fileBegin < fileEnd) {
        std::uint8_t current = *fileBegin;
        previousWasNewLine = (current == '\n');

        // Newlines always reset the state, this parser does not support multi-line statements
        if(previousWasNewLine) {
          isInsideQuote = false;
          encounteredComment = false;
          modelBuilder.EndLine(fileBegin + 1);
        } else if(isInsideQuote) { // Quotes make it ignore section and assignment characters
          if(current == '"') {
            isInsideQuote = false;
          }
        } else if(!encounteredComment) { // Otherwise, parse until comment encountered
          switch(current) {
            case ';':
            case '#': { encounteredComment = true; break; }
            case '[': { modelBuilder.BeginSection(fileBegin); break; }
            case ']': { /* modelBuilder.EndSection(fileBegin); */ break; }
            case '=': { /* modelBuilder.AddAssignment(fileBegin); */ break; }
            case '"': { isInsideQuote = true; break; }
            default: { break; }
          }
        }

        ++fileBegin;
      } // while fileBegin < fileEnd

      // If the file didn't end with a line break, consider the contents of
      // the file's final line as another line
      if(!previousWasNewLine) {
        modelBuilder.EndLine(fileEnd);
      }
    } // beauty scope
  }

  // ------------------------------------------------------------------------------------------- //

  template<typename TLine>
  TLine *IniDocumentModel::allocateLine(const std::uint8_t *contents, std::size_t length) {
    static_assert(std::is_base_of<Line, TLine>::value && u8"TLine inherits from Line");

    // Figure out how much space the structure would occupy in memory, including any padding
    // before the next array element. This will let us safely created a single-allocation
    // line instance that includes the actual line contents just behind the structure.
    std::size_t alignedFooterOffset;
    {
      const TLine *dummy = nullptr;
      alignedFooterOffset = (
        reinterpret_cast<const std::uint8_t *>(dummy + 1) -
        reinterpret_cast<const std::uint8_t *>(dummy)
      );
    }

    // Now allocate the memory block and place the line structure within it
    TLine *newLine;
    {
      std::unique_ptr<std::uint8_t[]> memory(new std::uint8_t[alignedFooterOffset + length]);

      newLine = reinterpret_cast<TLine *>(memory.get());
      newLine->Contents = memory.get() + alignedFooterOffset;
      newLine->Length = length;

      std::copy_n(contents, length, newLine->Contents);

      this->createdLinesMemory.insert(memory.get());

      memory.release(); // Disown the unique_ptr so the line's memory is not freed
    }

    return newLine;
  }

  // ------------------------------------------------------------------------------------------- //

  template<typename TLine>
  void IniDocumentModel::freeLine(TLine *line) {
    static_assert(std::is_base_of<Line, TLine>::value && u8"TLine inherits from Line");

    std::uint8_t *memory = reinterpret_cast<std::uint8_t *>(line);
    this->createdLinesMemory.erase(memory);
    delete[] memory;
  }

  // ------------------------------------------------------------------------------------------- //

}}} // namespace Nuclex::Support::Settings
