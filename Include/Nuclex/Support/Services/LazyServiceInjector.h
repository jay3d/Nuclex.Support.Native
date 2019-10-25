#pragma region CPL License
/*
Nuclex Native Framework
Copyright (C) 2002-2019 Nuclex Development Labs

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

#ifndef NUCLEX_SUPPORT_SERVICES_LAZYSERVICEINJECTOR_H
#define NUCLEX_SUPPORT_SERVICES_LAZYSERVICEINJECTOR_H

#include "Nuclex/Support/Config.h"
#include "Nuclex/Support/Services/ServiceContainer.h"
#include "Nuclex/Support/Services/ServiceLifetime.h"

#include <map>

namespace Nuclex { namespace Support { namespace Services {

  // ------------------------------------------------------------------------------------------- //

  /// <summary>The maximum number of constructor arguments that can be injected</summary>
  /// <remarks>
  ///   Increasing this value will result in (slightly) slower compiles. Though you might
  ///   want to reconsider your design if a single type consumes more than 8 services ;)
  /// </remarks>
  static constexpr std::size_t MaximumConstructorArgumentCount = 8;

  // ------------------------------------------------------------------------------------------- //

}}} // namespace Nuclex::Support::Services

#include "Nuclex/Support/Services/IntegerSequence.inl"
#include "Nuclex/Support/Services/Checks.inl"
#include "Nuclex/Support/Services/ConstructorSignature.inl"
#include "Nuclex/Support/Services/ConstructorSignatureDetector.inl"
#include "Nuclex/Support/Services/ServiceFactory.inl"

namespace Nuclex { namespace Support { namespace Services {

  // ------------------------------------------------------------------------------------------- //

  /// <summary>Binds services and initializes them via constructor injection</summary>
  /// <remarks>
  ///   This is a very simplified dependency injector that only supports global services
  ///   stored in shared_ptrs.
  /// </remarks>
  class LazyServiceInjector : public ServiceProvider {

    #pragma region class TypeInfoComparer

    /// <summary>Compares instances of std::type_info</summary>
    private: class TypeInfoComparer {

      /// <summary>Determines the relationship of two std::type_info instances</summary>
      /// <param name="left">Type info to compare on the left side</param>
      /// <param name="right">Type info to compare on the right side</param>
      /// <returns>True if the left side comes before the right side</returns>
      public: bool operator()(const std::type_info *left, const std::type_info *right) const {
        return left->before(*right) != 0;
      }

    };

    #pragma endregion // class TypeInfoComparer

    #pragma region class BindSyntax

    /// <summary>Provides the syntax for the fluent Bind() method</summary>
    public: template<typename TService> class BindSyntax {
      friend LazyServiceInjector;

      /// <summary>Initializes the syntax helper for binding services</summary>
      /// <param name="serviceInjector">Service injector on which services will be bound</param>
      protected: BindSyntax(LazyServiceInjector &serviceInjector) :
        serviceInjector(serviceInjector) {}

      /// <summary>Binds the service to a constructor-injected provider</summary>
      /// <typeparam name="TImplementation">Implementation of the service to use</typeparam>
      /// <remarks>
      ///   This binds the service to the specified service implementation
      /// </remarks>
      public: template<typename TImplementation> void To() {
        typedef Private::DetectConstructorSignature<TImplementation> ConstructorSignature;

        // Verify that the implementation actually implements the service
        static_assert(
          std::is_base_of<TService, TImplementation>::value,
          "Implementation must inherit from the service interface"
        );

        // Also verify that the implementation's constructor can be injected
        constexpr bool implementationHasInjectableConstructor = !std::is_base_of<
          Private::InvalidConstructorSignature, ConstructorSignature
        >::value;
        static_assert(
          implementationHasInjectableConstructor,
          "Implementation must have a constructor that can be dependency-injected "
          "(either providing a default constructor or using only std::shared_ptr arguments)"
        );

        // Implementation looks injectable, add the service factory method to the map
        const std::type_info &serviceTypeInfo = typeid(TService);
        this->serviceInjector.factories.insert(
          ServiceFactoryMap::value_type(
            &serviceTypeInfo,
            [](const ServiceProvider &serviceProvider) {
              typedef Private::ServiceFactory<TImplementation, ConstructorSignature> Factory;
              return Any(
                std::static_pointer_cast<TService>(Factory::CreateInstance(serviceProvider))
              );
            }
          )
        );

      }

      /// <summary>Binds the service to a factory method or functor used to create it</summary>
      /// <param name="factoryMethod">
      ///   Factory method that will be called to create the service
      /// </param>
      public: template<typename TResult, std::shared_ptr<TResult>(*TMethod)(const ServiceProvider &)>
      void ToFactoryMethod(){ 

        // Verify that whatever the factory method returns implements the service
        static_assert(
          std::is_base_of<TService, TResult>::value,
          "Factory method must return either the service type or one that "
          "inherits from it"
        );

        // Method does provide the service, add it to the map
        const std::type_info &serviceTypeInfo = typeid(TService);
        this->serviceInjector.factories.insert(
          ServiceFactoryMap::value_type(
            &serviceTypeInfo,
            [](const ServiceProvider &serviceProvider) {
              return Any(std::static_pointer_cast<TService>(TMethod(serviceProvider)));
            }
          )
        );

      }

      /// <summary>Binds the service to an already constructed service instance</summary>
      /// <param name="instance">Instance that will be returned for the service</param>
      public: void ToInstance(const std::shared_ptr<TService> &instance) {
        //const std::type_info &typeInfo = typeid(TService);
        //this->serviceInjector.services.Add(&typeInfo, Any(instance));
        this->serviceInjector.services.Add(instance);
      }

      /// <summary>Assumes that the service and its implementation are the same type</summary>
      /// <remarks>
      ///   For trivial services that don't have an interface separate from their implementation
      ///   class (or when you just have to provide some implementation everywhere),
      ///   use this method to say that the service type is a non-abstract class and
      ///   should be created directly.
      /// </remarks>
      public: void ToSelf() {
        typedef Private::DetectConstructorSignature<TService> ConstructorSignature;

        constexpr bool serviceHasInjectableConstructor = !std::is_base_of<
          Private::InvalidConstructorSignature, ConstructorSignature
        >::value;
        static_assert(
          serviceHasInjectableConstructor,
          "Self-bound service must not be abstract and requires a constructor "
          "that can be dependency-injected (either providing a default constructor or "
          "using only std::shared_ptr arguments)"
        );

        // Service looks injectable, add the service factory method to the map
        const std::type_info &serviceTypeInfo = typeid(TService);
        this->serviceInjector.factories.insert(
          ServiceFactoryMap::value_type(
            &serviceTypeInfo,
            [](const ServiceProvider &serviceProvider) {
              typedef Private::ServiceFactory<TService, ConstructorSignature> Factory;
              return Any(Factory::CreateInstance(serviceProvider));
            }
          )
        );

      }

      /// <summary>Service injector to which the binding will be added</summary>
      private: LazyServiceInjector &serviceInjector;

    };

    #pragma endregion // class BindSyntax

    #pragma region class ServiceStore

    /// <summary>Factory used to construct services or provide existing services</summary>
    private: class ServiceStore : public ServiceContainer {

      /// <summary>Frees all resources used by the service factory</summary>
      public: virtual ~ServiceStore() {}

      /// <summary>Looks up the specified service</summary>
      /// <param name="serviceType">Type of service that will be looked up</param>
      /// <returns>
      ///   The specified service as a shared_ptr wrapped in an <see cref="Any" />
      /// </returns>
      public: const Any &Get(const std::type_info &serviceType) const {
        return ServiceContainer::Get(serviceType);
      }

      /// <summary>Tries to look up the specified service</summary>
      /// <param name="serviceType">Type of service that will be looked up</param>
      /// <returns>An Any containing the service, if found, or an empty Any</returns>
      public: const Any &TryGet(const std::type_info &serviceType) const {
        return ServiceContainer::TryGet(serviceType);
      }

      /// <summary>Adds a service to the container</summary>
      /// <param name="serviceType">
      ///   Type of the service that will be added to the container
      /// </param>
      /// <param name="service">Object that provides the service</param>
      public: void Add(const std::type_info &serviceType, const Any &service) {
        ServiceContainer::Add(serviceType, service);
      }

      /// <summary>Removes a service from the container</summary>
      /// <param name="serviceType">
      ///   Type of the service that will be removed from the container
      /// </param>
      /// <returns>True if the service was found and removed</returns>
      public: bool Remove(const std::type_info &serviceType) {
        return ServiceContainer::Remove(serviceType);
      }

    };

    #pragma endregion // class ServiceStore

    /// <summary>Initializes a new service injector</summary>
    public: NUCLEX_SUPPORT_API LazyServiceInjector() = default;

    /// <summary>Destroys the service injector and frees all resources</summary>
    public: NUCLEX_SUPPORT_API virtual ~LazyServiceInjector() = default;

    /// <summary>Binds a provider to the specified service</summary>
    /// <returns>A syntax through which the provider to be bound can be selected</returns>
    public: template<typename TService> BindSyntax<TService> Bind() {
      return BindSyntax<TService>(*this);
    }

    // Unhide the templated Get method from the service provider
    using ServiceProvider::Get;

    // Unhide the templated TryGet method fro mthe service provider
    using ServiceProvider::TryGet;

    /// <summary>Looks up the specified service</summary>
    /// <param name="serviceType">Type of service that will be looked up</param>
    /// <returns>
    ///   The specified service as a shared_ptr wrapped in an <see cref="Any" />
    /// </returns>
    protected: NUCLEX_SUPPORT_API const Any &Get(
      const std::type_info &serviceType
    ) const override;

    /// <summary>Tries to look up the specified service</summary>
    /// <param name="serviceType">Type of service that will be looked up</param>
    /// <returns>An Any containing the service, if found, or an empty Any</returns>
    protected: NUCLEX_SUPPORT_API const Any &TryGet(
      const std::type_info &serviceType
    ) const override;

    /// <summary>Delegate for a factory method that creates a service</summary>
    private: typedef Any(*CreateServiceFunction)(const ServiceProvider &);

    /// <summary>Map of factories to create different services</summary> 
    private: typedef std::map<
      const std::type_info *, CreateServiceFunction, TypeInfoComparer
    > ServiceFactoryMap;

    // These are both mutable. Reasoning: the service injector acts as if all services
    // already exist, so 

    /// <summary>Factory methods to construct the various services</summary>
    private: mutable ServiceFactoryMap factories;
    /// <summary>Stores services that have already been initialized</summary>
    private: mutable ServiceStore services;

  };

  // ------------------------------------------------------------------------------------------- //

}}} // namespace Nuclex::Support::Services

#endif // NUCLEX_SUPPORT_SERVICES_LAZYSERVICEINJECTOR_H
