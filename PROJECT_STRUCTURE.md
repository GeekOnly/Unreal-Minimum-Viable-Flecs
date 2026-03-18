# AAA Project Structure & Clean Architecture Design

This document defines the professional project structure for `FlecsTest`, following **Unreal Engine 5 C++ Pro** standards and **Hexagonal Architecture** principles.

## 1. Directory Overview

```text
ProjectRoot/
├── Config/                 # Project & Engine settings
├── Content/                # Artists' playground (Filtered by Naming Conventions)
├── Plugins/                # Independent, reusable game features (Modular)
│   ├── Wind3DInteractive/  # Volumetric Wind Feature
│   └── Netcode/            # Quantum-Pulse Netcode System
├── Source/                 # Core Executive logic (The Glue)
│   ├── FlecsTest/          # Main Module
│   ├── FlecsTestTests/     # [NEW] Project-level Unit/Functional Tests
│   ├── FlecsTestServer.Target.cs
│   └── FlecsTest.Target.cs
├── Platforms/              # Platform-specific code (Android/Windows)
└── Docs/                   # Technical documentation & Roadmaps
```

## 2. Source Code Layering (Hexagonal)

To maintain a "10/10" netcode and physics system, we separate logic into three distinct layers:

### A. Domain Layer (Pure C++)
*   **Location**: `Source/FlecsTest/Public/Domain/`
*   **Rules**: No Unreal headers (no `AActor`, no `UObject`).
*   **Purpose**: Math, core gameplay rules, and state management that can be ported to **Rust** or tested in isolation.

### B. Application Layer (UObjects)
*   **Location**: `Source/FlecsTest/Public/Core/`
*   **Classes**: `UGameInstance`, `AGameMode`, `APlayerController`.
*   **Purpose**: Orchestrates game flow and interfaces between the Domain and Engine.

### C. Infrastructure Layer (Unreal Specific)
*   **Location**: `Source/FlecsTest/Public/Actors/`
*   **Classes**: `AActor`, `UStaticMeshComponent`, `UNiagaraComponent`.
*   **Purpose**: Rendering, Physics (Chaos), and Input handling.

---

## 3. Best Practices (SKILL.md Compliance)

### UObject Hygiene
*   **Naming**: `A` for Actors, `U` for Objects/Components, `F` for Structs, `I` for Interfaces, `E` for Enums.
*   **Memory**: Every `UObject*` pointer MUST be a `UPROPERTY()` to avoid Garbage Collection deletion.
*   **Soft References**: Use `TSoftObjectPtr<UStaticMesh>` instead of `UStaticMesh*` for assets to prevent massive load-time "Chains".

### Performance Standards
*   **Tick Management**: `bCanEverTick = false` by default for all Actors. Use `GetWorldTimerManager()` for repeating logic.
*   **Casting**: Cache components in `BeginPlay()` using `FindComponentByClass<T>()` instead of casting every frame.

---

## 4. Content Naming Conventions (The Artist's Rule)

To keep the `Content/` folder clean for AAA production:
*   **M_**: Materials (e.g., `M_Master_Foliage`)
*   **T_**: Textures (e.g., `T_Wind_Noise_D`)
*   **S_**: Static Meshes
*   **SK_**: Skeletal Meshes
*   **WBP_**: Widget Blueprints

---

## 5. Deployment & Build Targets

| Target | Configuration | Output | Purpose |
|--------|---------------|--------|---------|
| **FlecsTest** | Development | EXE / AAB | Playing & Editor Testing |
| **FlecsTestServer** | Shipping | Linux Headless | Production Dedicated Server |

---

> [!TIP]
> **Modularity is Key**: If a feature (like Wind or Netcode) can exist on its own, always put it in the `Plugins/` folder. This makes the `Source/` folder extremely light and easy to maintain.
