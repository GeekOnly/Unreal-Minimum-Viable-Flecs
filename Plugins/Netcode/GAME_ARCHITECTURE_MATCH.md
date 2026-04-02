# Game Architecture Match: Choosing the Best Case

Selecting the right server architecture depends entirely on your game's genre and core mechanics. This guide identifies the "Best Case" for each major category.

---

## 1. Fast-Paced Shooters (FPS / TPS)
*   **Best Case**: **Hybrid (Unreal Server + Rust Sub-tick)**
*   **Reasoning**: 
    *   **Physics**: Needs 100% parity between client and server for hit detection and "Pead-peaking" advantage. Unreal's Chaos is built for this.
    *   **Competitive**: Rust handles the sub-tick math to ensure the player with 10ms ping doesn't lose a headshot to quantization errors.

## 2. MMOs / Social Spaces (1,000+ Players)
*   **Best Case**: **Pure Rust (ECS-based)**
*   **Reasoning**: 
    *   **Density**: Unreal's overhead per actor is too high for thousands of players on one shard.
    *   **Cost**: You need to run huge world instances on cheap Bare Metal. Rust's memory safety and low footprint are essential here.
    *   **Physics**: Usually "Tab-target" or simple collision, so Physics Drift is not a dealbreaker.

## 3. High-Density PvE / "Horde" Games
*   **Best Case**: **Hybrid (Unreal Server + Rust/Flecs AI)**
*   **Reasoning**:
    *   **AI Density**: Unreal's `AAIController` is heavy. Offloading 1,000+ NPC "Brains" to Rust/Flecs allows for huge hordes.
    *   **Sync**: Use Unreal for the Boss/Elite physics and P2P Mesh for the generic "Minion" movement to save bandwidth.

## 4. Physics-Heavy / Racing Games
*   **Best Case**: **Unreal Dedicated Server (Full Chaos)**
*   **Reasoning**:
    *   **Complexity**: Re-implementing vehicle suspension and tire friction in a custom Rust server is a massive undertaking.
    *   **Engine Integration**: Unreal provides the best tools for tweaking physics data visually in the Editor.

## 5. Casual / Turn-Based / Strategy
*   **Best Case**: **Pure Rust or Serverless (Cloud Functions)**
*   **Reasoning**:
    *   **State over Physics**: These games don't need real-time collision. They need stable state management and database integrity.
    *   **Scalability**: Rust or Go can scale to millions of users with almost zero idle cost.

---

## Summary Decision Matrix

| Genre | Architecture | Priority |
| :--- | :--- | :--- |
| **Brawl Stars style (PvP)** | **Hybrid** | Latency & Precision |
| **Genshin style (PvE Co-op)** | **Hybrid** | AI Density & Consistency |
| **WoW style (MMO)** | **Pure Rust** | Scalability & CCU |
| **Rocket League style** | **Unreal Dedicated**| Deterministic Physics |
| **Clash Royale style** | **Pure Rust** | Logic & Cost |

---

> [!TIP]
> **The 2026 Trend**: Most modern AAA studios are moving toward the **Hybrid** model. They use the Engine for what it’s good at (Visuals/Physics) and Rust for what it's good at (Reliability/Networking/AI).
