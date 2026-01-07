// GENERATED CODE - DO NOT MODIFY BY HAND

part of 'api_provider.dart';

// **************************************************************************
// RiverpodGenerator
// **************************************************************************

String _$apiClientHash() => r'a0eb8837fc3cfc2ac414f9bdd23761e2186e929e';

/// See also [apiClient].
@ProviderFor(apiClient)
final apiClientProvider = AutoDisposeProvider<ApiClient>.internal(
  apiClient,
  name: r'apiClientProvider',
  debugGetCreateSourceHash:
      const bool.fromEnvironment('dart.vm.product') ? null : _$apiClientHash,
  dependencies: null,
  allTransitiveDependencies: null,
);

@Deprecated('Will be removed in 3.0. Use Ref instead')
// ignore: unused_element
typedef ApiClientRef = AutoDisposeProviderRef<ApiClient>;
String _$authenticatedApiClientHash() =>
    r'1b702b12e0b4d1e928fc0660de85c04bfb5eed30';

/// See also [authenticatedApiClient].
@ProviderFor(authenticatedApiClient)
final authenticatedApiClientProvider =
    AutoDisposeFutureProvider<ApiClient>.internal(
  authenticatedApiClient,
  name: r'authenticatedApiClientProvider',
  debugGetCreateSourceHash: const bool.fromEnvironment('dart.vm.product')
      ? null
      : _$authenticatedApiClientHash,
  dependencies: null,
  allTransitiveDependencies: null,
);

@Deprecated('Will be removed in 3.0. Use Ref instead')
// ignore: unused_element
typedef AuthenticatedApiClientRef = AutoDisposeFutureProviderRef<ApiClient>;
// ignore_for_file: type=lint
// ignore_for_file: subtype_of_sealed_class, invalid_use_of_internal_member, invalid_use_of_visible_for_testing_member, deprecated_member_use_from_same_package
