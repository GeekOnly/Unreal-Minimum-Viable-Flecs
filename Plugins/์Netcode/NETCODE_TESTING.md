# Netcode Automated Testing Strategy

This document outlines the professional testing architecture for the `Netcode` plugin, following **Unreal Engine Automation Framework** standards.

## 1. Testing Hierarchy

| Test Type | Target Layer | Module | Description |
|-----------|--------------|--------|-------------|
| **Unit Tests** | Domain (C++/Rust) | `NetcodeTests` | Test Bit-packing, Sub-tick Math, and Prediction algorithms without the Engine. |
| **Integration Tests** | Application (Components) | `NetcodeTests` | Test `UNetcodePhysicsComponent` interaction with Unreal Physics. |
| **Functional Tests** | Infrastructure (Net) | `NetcodeTests` | End-to-end testing of RPCs and State Sync between a Mock Server and Client. |

## 2. Unit Test Example (Domain)
Tests are written using the `IMPLEMENT_SIMPLE_AUTOMATION_TEST` macro.

```cpp
// Source/NetcodeTests/Private/Domain/BitPackingTest.cpp
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBitPackingTest, "Netcode.Domain.BitPacking", EAutomationTestFlags::ApplicationContextMask | EAutomationTestFlags::ProductFilter)

bool FBitPackingTest::RunTest(const FString& Parameters)
{
    FNetcodeBitWriter Writer;
    Writer.WriteBit(true);
    Writer.WriteInt(42, 6); // 42 using only 6 bits

    FNetcodeBitReader Reader(Writer.GetData());
    TestTrue("Read Bit Correct", Reader.ReadBit());
    TestEqual("Read Int Correct", Reader.ReadInt(6), 42);
    
    return true;
}
```

## 3. High-Fidelity Replication Testing
Using **`FNetworkAutomationTest`**, we simulate varying network conditions:
*   **Latency Simulation**: Inject 100ms-500ms lag.
*   **Packet Loss**: Simulate 5%-20% loss to verify **Neural PLC** recovery.
*   **Jitter**: Test Sub-tick precision under unstable timing.

---

## 4. Best Practices (SKILL.md Compliance)

1.  **Isolated Modules**: Keep tests in a separate `NetcodeTests` module. This ensures that testing code is NEVER shipped to the end-user (reduces APK size).
2.  **Mocking Interfaces**: Use `INetcodeSession` interfaces to mock the network, allowing tests to run in a standalone environment without a real EOS connection.
3.  **Continuous Integration (CI)**: All tests must pass in the **Linux Server Build** pipeline before merging.

---

> [!TIP]
> **Automation Tool**: Run all tests via the **Session Frontend** in Unreal Editor (`Window > (General) > Session Frontend`) to see visual results and performance metrics.
