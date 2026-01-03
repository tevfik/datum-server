import 'package:flutter_test/flutter_test.dart';
import 'package:mockito/mockito.dart';
import 'package:mockito/annotations.dart';
import 'package:datum_camera_app/providers/auth_provider.dart';
import 'package:datum_camera_app/api_client.dart';
import 'package:shared_preferences/shared_preferences.dart';

// Generate Mocks
@GenerateMocks([ApiClient])
import 'auth_provider_test.mocks.dart';

void main() {
  late MockApiClient mockApiClient;
  late AuthProvider authProvider;

  setUp(() {
    mockApiClient = MockApiClient();
    SharedPreferences.setMockInitialValues({});
    authProvider = AuthProvider(apiClient: mockApiClient);
  });

  group('AuthProvider Tests', () {
    test('login success sets token and notifies listeners', () async {
      // Arrange
      when(mockApiClient.login('test@example.com', 'password'))
          .thenAnswer((_) async => 'fake_token');
      
      // Act
      final result = await authProvider.login('test@example.com', 'password');

      // Assert
      expect(result, true);
      expect(authProvider.isAuthenticated, true);
      verify(mockApiClient.login('test@example.com', 'password')).called(1);
      verify(mockApiClient.setToken('fake_token')).called(1);
    });

    test('login failure returns false', () async {
      // Arrange
      when(mockApiClient.login('wrong', 'pass'))
          .thenThrow(Exception('Login failed'));
      
      // Act
      final result = await authProvider.login('wrong', 'pass');

      // Assert
      expect(result, false);
      expect(authProvider.isAuthenticated, false);
    });

    test('logout clears token', () async {
      // Arrange
      when(mockApiClient.login('test', 'pass')).thenAnswer((_) async => 'token');
      await authProvider.login('test', 'pass');
      
      // Act
      await authProvider.logout();

      // Assert
      expect(authProvider.isAuthenticated, false);
      verify(mockApiClient.clearToken()).called(1);
    });
  });
}
