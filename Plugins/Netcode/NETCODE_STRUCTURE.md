# Netcode Plugin Internal Structure Design

This document defines the internal architecture of the `Netcode` plugin, adhering to **Unreal Engine 5 C++ Pro** standards and **Hexagonal Architecture**.

## 1. Directory Structure

```text
Plugins/Netcode/
├── Source/
│   ├── Netcode/                # Primary Runtime Module
│   │   ├── Public/             # API Headers (Included by other modules)
│   │   │   ├── Domain/         # Pure Logic (Rust-ready, no UObjects)
│   │   │   ├── Core/           # Application Logic (UObjects/Components)
│   │   │   ├── Interfaces/     # I-prefixed interfaces for ports
│   │   │   └── Netcode.h       # Module definition
│   │   ├── Private/            # Implementation (.cpp)
│   │   │   ├── Subsystems/     # Network & Session Managers
│   │   │   └── Components/     # Actor Component implementations
│   │   └── Netcode.Build.cs    # Dependencies (OnlineSubsystem, Chaos, etc.)
│   ├── NetcodeFFI/             # Rust Foreign Function Interface (Bridge)
│   │   ├── Public/             # C++ wrappers for Rust functions
│   │   └── Private/            # The actual cxx/autocxx bridge code
│   ├── NetcodeTests/           # [NEW] Automated Unit & Integration Tests
│   └── NetcodeEditor/          # Editor-only visualization & debug tools
├── Shaders/                    # Custom .usf/.ush for Neural PLC or debug
├── Resources/                  # Icons, Dockerfiles, and Config templates
└── Netcode.uplugin             # Plugin Manifest
```

## 2. Layering within the Plugin

### A. Domain Layer (The "Boltz" Core)
- **Path**: `Source/Netcode/Public/Domain/`
- **Standard**: Pure C++ (Structs/Enums). No `AActor`.
- **Purpose**: Implements the **Resimulation Logic**, **Bit-packing algorithms**, and **Input Buffering**. This is the layer that will eventually be translated to Rust.

### B. Core Layer (The "Quantum" Application)
- **Path**: `Source/Netcode/Public/Core/`
- **Classes**: 
    - `UNetcodePhysicsComponent`: The main interface for Actors to use netcode.
    - `UNetcodeSessionManager`: Handles EOS/P2P sessions.
- **Purpose**: Maps Unreal Engine events (Physics ticks, RPCs) to the Domain logic.

### C. Infrastructure Layer (Engine Adapters)
- **Path**: `Source/Netcode/Private/Subsystems/`
- **Purpose**: Unreal-specific implementations of network transport (UDP/WebRTC).

---

## 3. FFI Strategy (Rust Bridge)

The **`NetcodeFFI`** module acts as an isolated sandbox for Rust integration:
- It uses **`cxx`** to share memory-safe types between C++ and Rust.
- It prevents the main `Netcode` module from being "polluted" with Rust-specific build logic.

---

## 4. Coding Standards (SKILL.md Compliance)

1.  **Component Hygiene**: `UNetcodePhysicsComponent` must have `bWantsInitializeComponent = true` to cache dependencies early.
2.  **Thread Safety**: Since physics runs on the **Async Physics Thread**, all data passed to `QuantumTick` must be through immutable `F` structs (`FNetcodeInput`, `FNetcodeState`).
3.  **Logging**: Use `DECLARE_LOG_CATEGORY_EXTERN(LogNetcode, Log, All);` for professional traceability.

---

> [!IMPORTANT]
> **Modularity**: Every sub-system (Session, Sync, Prediction) within the plugin should be reachable via an Interface (`INetcodeSession`) to allow for easy mocking during unit tests.
