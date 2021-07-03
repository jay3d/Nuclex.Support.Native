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

#ifndef NUCLEX_SUPPORT_SETTINGS_INIDOCUMENTMODEL_H
#define NUCLEX_SUPPORT_SETTINGS_INIDOCUMENTMODEL_H

#include "Nuclex/Support/Config.h"

#include <vector> // for std::vector
#include <string> // for std::string
#include <cstdint> // for std::uint8_t

#include <unordered_map> // for std::unordered_map
#include <unordered_set> // for std::unordered_set

namespace Nuclex { namespace Support { namespace Settings {

  // ------------------------------------------------------------------------------------------- //

  class IniDocumentModel {

    /// <summary>Initializes a new empty .ini file document model</summary>
    public: IniDocumentModel();

    /// <summary>
    ///   Initializes a new .ini file document model parsing the specified file contents
    /// </summary>
    /// <param name="fileContents">The whole contents of an .ini file</param>
    /// <param name="byteCount">Lenght of the .ini file in bytes</param>
    public: IniDocumentModel(const std::uint8_t *fileContents, std::size_t byteCount);

    /// <summary>Frees all memory owned by the instance</summary>
    public: ~IniDocumentModel();

    #pragma region struct Line

    /// <summary>An arbitrary line from an .ini file</summary>
    protected: struct Line {

      /// </summary>Pointer to the previous line</summary>
      public: Line *Previous;
      /// </summary>Pointer to the next line</summary>
      public: Line *Next;

      /// <summary>The text contained in this line, including CR or CR-LF</summary>
      public: std::uint8_t *Contents;
      /// <summary>Length of the line in bytes</summary>
      public: std::size_t Length;

    };

    #pragma endregion // struct Line

    #pragma region struct SectionLine

    /// <summary>A line in an .ini file declaring a section</summary>
    protected: struct SectionLine : public Line {

      /// <summary>Byte index at which the section name begins</summary>
      public: std::size_t NameStartIndex;
      /// <summary>Length of the section name in bytes</summary>
      public: std::size_t NameLength;

    };

    #pragma endregion // struct SectionLine

    #pragma region struct PropertyLine

    /// <summary>A line in an .ini file containing a property assignment</summary>
    protected: struct PropertyLine : public Line {

      /// <summary>Byte index at which the property name begins</summary>
      public: std::size_t NameStartIndex;
      /// <summary>Length of the property name in bytes</summary>
      public: std::size_t NameLength;
      /// <summary>Byte index at which the property's value begins</summary>
      public: std::size_t ValueStartIndex;
      /// <summary>Length of the property's value in bytes</summary>
      public: std::size_t ValueLength;

    };

    #pragma endregion // struct PropertyLine

    // Internal helper to estimate the memory needed to hold the parsed document model
    private: class MemoryEstimator;

    /// <summary>Estimates the amount of memory required for the document model</summary>
    /// <param name="fileContents">Buffer holding the entire .ini file in memory</param>
    /// <param name="byteCount">Size of the .ini file in bytes</param>
    /// <returns>The estimated amount of memory required to build the document model</returns>
    private: static std::size_t estimateRequiredMemory(
      const std::uint8_t *fileContents, std::size_t byteCount
    );

    /// <summary>Parses an .ini file into this easily accessible document model</summary>
    /// <param name="fileContents">The whole contents of an .ini file</param>
    /// <param name="byteCount">Lenght of the .ini file in bytes</param>
    private: void parse(const std::uint8_t *fileContents, std::size_t byteCount);

    /// <summary>Allocates memory for a single line</summary>
    /// <typeparam name="TLine">Type of line that will be allocated</typeparam>
    /// <param name="contents">The bytes this line consists of, including CR / CR-LF</param>
    /// <param name="length">Length of the line in bytes</param>
    /// <returns>The new line</returns>
    private: template<typename TLine>
    TLine *allocateLine(const std::uint8_t *contents, std::size_t length);

    /// <summary>Frees memory for a single line</summary>
    /// <typeparam name="TLine">Type of line that will be freed</typeparam>
    /// <param name="line">Line instance that will be freed</param>
    private: template<typename TLine>
    void freeLine(TLine *line);

    /// <summary>Memory holding all Line instances from when the .ini file was loaded</summary>
    /// <remarks>
    ///   Instead of allocating lines individually, this document model allocates a big memory
    ///   chunk that holds all line instances and their respective text, too. This avoids
    ///   memory fragmentation and is fairly efficient as usually, .ini files aren't completely
    ///   restructured during an application run.
    /// </remarks>
    private: std::uint8_t *loadedLinesMemory;
    /// <summary>Memory for all Line instances that were created after loading</summary>
    private: std::unordered_set<std::uint8_t *> createdLinesMemory;

    /// <summary>Map from property name to the lines containing a property</summary>
    typedef std::unordered_map<std::string, PropertyLine *> PropertyMap;
    /// <summary>Map from section name to a map holding the properties in the section</summary>
    typedef std::unordered_map<std::string, PropertyMap *> SectionMap;

    /// <summary>Pointer to the first line, useful to reconstruct the file</summary>
    private: Line *firstLine;
    /// <summary>Map allowing quick access to all the sections in the .ini file</summary>
    /// <remarks>
    ///   The global section (containing all properties before the first section declaration)
    ///   is nameless and always present.
    /// </remarks>
    private: SectionMap sections;

    /// <summary>Should there be spaces before and after the equals sign?</summary>
    private: bool hasSpacesAroundAssignment;
    /// <summary>Should property assignments be padded with empty lines between them?</summary>
    private: bool hasEmptyLinesBetweenProperties;

  };

  // ------------------------------------------------------------------------------------------- //

}}} // namespace Nuclex::Support::Settings

#endif // NUCLEX_SUPPORT_SETTINGS_INIDOCUMENTMODEL_H
