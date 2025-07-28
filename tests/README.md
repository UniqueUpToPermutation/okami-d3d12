# Engine Testing Framework

This project includes a comprehensive testing framework using Google Test (gtest) to ensure code quality and reliability.

## Test Structure

### Test Files

- **`tests/world_test.cpp`** - Comprehensive unit tests for the World class including:
  - Entity creation and management
  - Hierarchy operations (parent/child relationships)
  - Iterator functionality (children, ancestors, descendants)
  - Edge cases and error handling
  - Circular dependency prevention

- **`tests/engine_test.cpp`** - Tests for engine components including:
  - Engine module system
  - Signal handling system
  - Interface collection
  - Error handling utilities
  - ScopeGuard utility

- **`tests/integration_test.cpp`** - Integration and system tests including:
  - Performance tests for large hierarchies
  - Memory management tests
  - Stress tests with random operations
  - Regression tests for known issues
  - Thread safety tests (disabled by default)

- **`tests/benchmark_test.cpp`** - Performance benchmarks including:
  - Entity creation/removal benchmarks
  - Hierarchy traversal performance
  - Random operation stress testing
  - Memory usage testing

## Building and Running Tests

### Prerequisites

Make sure you have the following dependencies installed via vcpkg:
```bash
vcpkg install gtest
```

### Building Tests

```bash
# Configure with CMake
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE="<vcpkg-root>/scripts/buildsystems/vcpkg.cmake"

# Build all targets including tests
cmake --build build

# Or build just the test executable
cmake --build build --target EngineTests
```

### Running Tests

#### Run All Tests
```bash
# Using CTest
cmake --build build --target run_tests

# Or directly
./build/EngineTests
```

#### Run Specific Test Suites
```bash
# World tests only
cmake --build build --target run_world_tests

# Engine tests only  
cmake --build build --target run_engine_tests

# Performance tests only
cmake --build build --target run_performance_tests
```

#### Run Tests with Filters
```bash
# Run specific test cases
./build/EngineTests --gtest_filter="WorldTest.CreateEntityTest"

# Run all tests matching a pattern
./build/EngineTests --gtest_filter="WorldTest.*"

# Exclude specific tests
./build/EngineTests --gtest_filter="-PerformanceTest.*"
```

#### Run Tests with Verbose Output
```bash
./build/EngineTests --gtest_verbose

# Or with CTest
ctest --output-on-failure --verbose
```

## Test Categories

### Unit Tests
- Test individual components in isolation
- Fast execution (< 1ms per test typically)
- Focus on correctness and edge cases
- Examples: `WorldTest.*`, `ErrorTest.*`

### Integration Tests  
- Test multiple components working together
- Moderate execution time
- Focus on component interactions
- Examples: `IntegrationTest.*`

### Performance Tests
- Measure execution time and resource usage
- Longer execution time
- Focus on scalability and performance regressions
- Examples: `PerformanceTest.*`, `WorldBenchmark.*`

### Regression Tests
- Test specific bug fixes and known issues
- Prevent reintroduction of bugs
- Examples: `RegressionTest.*`

## Writing New Tests

### Basic Test Structure
```cpp
#include <gtest/gtest.h>
#include "../your_header.hpp"

class YourClassTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup before each test
    }

    void TearDown() override {
        // Cleanup after each test
    }

    // Test fixtures/data
};

TEST_F(YourClassTest, TestName) {
    // Test implementation
    EXPECT_EQ(expected, actual);
    ASSERT_TRUE(condition);
}
```

### Test Naming Conventions
- Test class: `ComponentNameTest`
- Test method: `FunctionalityBeingTestedTest`
- Use descriptive names that explain what is being tested

### Assertion Guidelines
- Use `EXPECT_*` for non-fatal assertions
- Use `ASSERT_*` for fatal assertions (stops test execution)
- Common assertions:
  - `EXPECT_EQ(expected, actual)` - equality
  - `EXPECT_NE(a, b)` - inequality  
  - `EXPECT_TRUE(condition)` - boolean true
  - `EXPECT_FALSE(condition)` - boolean false
  - `EXPECT_LT(a, b)` - less than
  - `EXPECT_GT(a, b)` - greater than
  - `EXPECT_NO_THROW(statement)` - no exception thrown
  - `EXPECT_THROW(statement, exception_type)` - specific exception thrown

## Test Data and Fixtures

### Using Test Fixtures
Test fixtures allow sharing setup/teardown code between tests:

```cpp
class WorldTest : public ::testing::Test {
protected:
    void SetUp() override {
        world = std::make_unique<World>();
    }

    std::unique_ptr<World> world;
};
```

### Parameterized Tests
For testing with multiple input values:

```cpp
class ParameterizedTest : public ::testing::TestWithParam<int> {};

TEST_P(ParameterizedTest, TestWithParameter) {
    int value = GetParam();
    // Test with value
}

INSTANTIATE_TEST_SUITE_P(TestName, ParameterizedTest, 
    ::testing::Values(1, 2, 3, 4, 5));
```

## Continuous Integration

Tests are designed to be run in CI environments:

- All tests should complete within 5 minutes
- No external dependencies required (beyond vcpkg packages)
- Tests are deterministic and repeatable
- Performance tests include reasonable timeouts

## Performance Test Guidelines

- Performance tests output timing information
- Include reasonable performance expectations
- Use fixed seeds for random number generators for reproducibility
- Test both average case and worst case scenarios

## Debugging Tests

### Running Tests in Debugger
```bash
# Build debug version
cmake -B build -S . -DCMAKE_BUILD_TYPE=Debug

# Run specific test in debugger
gdb ./build/EngineTests
(gdb) run --gtest_filter="WorldTest.CreateEntityTest"
```

### Test Output
- Tests output timing information for performance tests
- Failed tests show detailed assertion information
- Use `--gtest_verbose` for additional output

## Best Practices

1. **Test Independence** - Each test should be independent and not rely on other tests
2. **Clear Test Names** - Test names should clearly describe what is being tested
3. **Fast Tests** - Keep unit tests fast (< 1ms each when possible)
4. **Good Coverage** - Test both success and failure cases
5. **Edge Cases** - Test boundary conditions and edge cases
6. **Documentation** - Comment complex test logic
7. **Cleanup** - Always clean up resources in test teardown

## Test Metrics

The test suite includes:
- **100+ unit tests** covering core functionality
- **Performance benchmarks** for critical operations
- **Integration tests** for component interactions
- **Regression tests** for known issues
- **Edge case testing** for robustness

Run `./build/EngineTests --gtest_list_tests` to see all available tests.