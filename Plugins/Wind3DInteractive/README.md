# Wind3DInteractive Plugin

ระบบลม 3D แบบ volumetric สำหรับ Unreal Engine 5.7 ขับเคลื่อนด้วย [Flecs ECS](https://www.flecs.dev/) (v3.2.12)
แรงบันดาลใจจากระบบลมใน *God of War* — ลมส่งผลกระทบต่อใบไม้ใบหญ้าแบบ real-time บน CPU grid

---

## โครงสร้าง Modules

| Module | ประเภท | หน้าที่ |
|--------|--------|---------|
| `FlecsLibrary` | Runtime | Flecs ECS C library wrapper (amalgamated) |
| `Wind3DInteractive` | Runtime | Wind grid, subsystem, actors, ECS systems |
| `Wind3DInteractiveEditor` | Editor | Gizmo visualizer สำหรับ Wind Motor |

---

## การติดตั้ง

1. ใส่โฟลเดอร์ `Plugins/Wind3DInteractive/` ใน project
2. เปิด `.uproject` → Plugins → เปิดใช้งาน **Wind3DInteractive**
3. Rebuild project (right-click `.uproject` → Generate Visual Studio project files)
4. เพิ่ม dependency ใน `YourGame.Build.cs`:

```csharp
PrivateDependencyModuleNames.AddRange(new string[] { "Wind3DInteractive" });
```

---

## แนวคิดหลัก

### Wind Grid
- กริด 3D แบบ voxel บน CPU — ค่า default: **16 × 16 × 8 cells**, cell ละ **200 cm**
- ครอบคลุมพื้นที่ 3200 × 3200 × 1600 cm (32m × 32m × 16m)
- ค่าลมในแต่ละ cell เก็บเป็น `FVector` (velocity) และ `float` (turbulence)
- Sample ด้วย **trilinear interpolation** เพื่อให้ลมไหลเนียน
- Decay กลับสู่ ambient wind ทุก tick

### Wind Motor
- Actor ที่ปล่อยลมเข้า grid — รองรับ 3 รูปทรง และ 3 รูปแบบการปล่อยลม
- แต่ละ motor เป็น Flecs entity มี component `FWindMotorData`
- Gizmo wireframe แสดงใน editor viewport โดยอัตโนมัติ

### Foliage Response
- ลงทะเบียน HISM instance แต่ละตัวเป็น Flecs entity
- ECS system sample ลมที่ตำแหน่ง foliage → คำนวณ damped harmonic oscillator
- เขียนผลลัพธ์ผ่าน **Custom Primitive Data** (CPD) ไปยัง Material

---

## การใช้งาน

### 1. วาง Wind Motor Actor ใน Level

ลาก **Wind Motor Actor** (`Place Actors` → ค้นหา "Wind Motor") วางในฉาก

**Properties ที่ปรับได้:**

| Property | Default | คำอธิบาย |
|----------|---------|-----------|
| `Shape` | Sphere | รูปทรงของ motor (Sphere / Cylinder / Cone) |
| `EmissionType` | Directional | รูปแบบปล่อยลม (Directional / Omni / Vortex) |
| `Radius` | 300 cm | รัศมีของ motor |
| `Height` | 500 cm | ความสูง (สำหรับ Cylinder / Cone) |
| `Strength` | 500 | ความแรงลม (0–2000) |
| `VortexAngularSpeed` | 1.0 | ความเร็วหมุน (เฉพาะ Vortex) |
| `bEnabled` | true | เปิด/ปิด motor |

**Editor Gizmo:**
- Sphere → 3 วงกลมตั้งฉากสีฟ้า
- Cylinder → วงกลมบน-ล่าง + เส้นตั้ง
- Cone → วงกลมฐาน + เส้นจาก apex
- ลูกศรสีเหลืองแสดงทิศทางปล่อยลม

---

### 2. ดู Wind Field Debug

เปิด Unreal Console (`~`) แล้วพิมพ์:

```
Wind3D.ShowWindField 1
```

จะเห็นลูกศรสีเขียว-แดงในทุก cell ของ grid ใน viewport
ปิดด้วย `Wind3D.ShowWindField 0`

---

### 3. Blueprint API (UWindSubsystem)

เข้าถึงผ่าน **Get Game Instance Subsystem** → เลือก `Wind Subsystem`

#### ตั้งค่า Grid
```
SetupWindGrid(
  Origin = (0, 0, 0),
  CellSize = 200.0,
  SizeX = 16,
  SizeY = 16,
  SizeZ = 8
)
```
> เรียกครั้งเดียวใน BeginPlay ของ GameMode / Level Blueprint

#### ตั้ง Ambient Wind
```
SetAmbientWind(Velocity = (100, 0, 0))
```
ลมพื้นหลังที่ grid decay ไปหา (cm/s)

#### สร้าง Wind Motor ด้วย Blueprint
```
Handle = RegisterWindMotor(
  Position = GetActorLocation(),
  Direction = GetActorForwardVector(),
  Strength = 800.0,
  Radius = 400.0,
  Shape = Sphere,
  EmissionType = Omni
)
```

#### อัปเดตตำแหน่ง Motor
```
UpdateWindMotor(Handle, NewPosition, NewDirection)
```

#### ลบ Motor
```
UnregisterWindMotor(Handle)
```

#### Query ลม ณ ตำแหน่งใดก็ได้
```
WindVelocity = QueryWindAtPosition(WorldPosition)
```
คืนค่า `FVector` (cm/s) — ใช้ใน Blueprint เพื่อ effect เช่น particle direction

---

### 4. ลงทะเบียน Foliage (C++)

```cpp
UWindSubsystem* WindSys = GetGameInstance()->GetSubsystem<UWindSubsystem>();

FWindEntityHandle Handle = WindSys->RegisterFoliageInstance(
    MyHISMComponent,   // UHierarchicalInstancedStaticMeshComponent*
    InstanceIndex,     // int32 - index ใน HISM
    WorldLocation,     // FVector - ตำแหน่ง instance
    1.0f,              // Sensitivity  - ความไวต่อลม
    10.0f,             // Stiffness    - ความแข็ง (spring constant)
    2.0f,              // Damping      - การหน่วง
    0,                 // CPDSlotDisplace  - Custom Primitive Data slot สำหรับ displacement
    1                  // CPDSlotTurbulence - CPD slot สำหรับ turbulence
);
```

**Material Setup:**
ใช้ node `VertexInterpolator` + `CustomPrimitiveData[0]` สำหรับ displacement
ใช้ `CustomPrimitiveData[1]` สำหรับ turbulence intensity

---

### 5. C++ Integration

```cpp
// เข้าถึง Flecs world โดยตรง
flecs::world* ECS = WindSys->GetEcsWorld();

// Query เอง
ECS->each([](FWindMotorData& Motor) {
    // ...
});

// เข้าถึง Wind Grid โดยตรง
const FWindGrid& Grid = WindSys->GetWindGrid();
FVector WindVel = Grid.SampleVelocityAt(MyWorldPos);
float Turb = Grid.SampleTurbulenceAt(MyWorldPos);
```

---

## Wind Motor Shapes

### Sphere
ปล่อยลมรอบทิศจากจุดศูนย์กลาง เหมาะสำหรับ explosion, ventilation shaft

```
Shape = Sphere
EmissionType = Omni
Radius = 500
Strength = 600
```

### Cylinder
เหมาะสำหรับ tunnel, corridor — ลมไหลตามแกน Z

```
Shape = Cylinder
EmissionType = Directional
Radius = 200
Height = 800
Strength = 400
```

### Cone
เหมาะสำหรับ fan, nozzle — ลมพุ่งออกจาก apex ตามแกน X

```
Shape = Cone
EmissionType = Directional
Radius = 300
Height = 600
Strength = 700
```

## Emission Types

| Type | ลักษณะ | Use Case |
|------|--------|---------|
| `Directional` | ลมพุ่งตาม Forward vector ของ actor | พัดลม, ลมผ่าน corridor |
| `Omni` | ลมแผ่รอบทิศ | Explosion, heat rising |
| `Vortex` | ลมหมุนเวียน | Tornado, whirlpool |

---

## Wind Grid Configuration

| Parameter | Default | คำแนะนำ |
|-----------|---------|---------|
| CellSize | 200 cm | เล็กลง = ละเอียดขึ้น แต่ช้าลง |
| SizeX/Y | 16 | ขยายตามขนาด level |
| SizeZ | 8 | พอสำหรับ foliage layer |

**ตัวอย่าง Level ขนาดใหญ่:**
```
SetupWindGrid(Origin=(-3200,-3200,0), CellSize=400, SizeX=32, SizeY=32, SizeZ=16)
```

---

## Flecs Explorer (Debug)

subsystem เปิด Flecs REST API ไว้ที่ port **27750** อัตโนมัติ
เปิด browser ไปที่ `https://www.flecs.dev/explorer/` แล้วกด Connect
จะเห็น entity ทั้งหมด, component values, และ system performance

---

## Module Dependencies

สำหรับ project module ที่ต้องการใช้ plugin นี้:

```csharp
// YourModule.Build.cs
PrivateDependencyModuleNames.AddRange(new string[]
{
    "FlecsLibrary",       // ต้องใส่ตรงๆ เพราะ UBT ไม่ propagate transitive DLL deps
    "Wind3DInteractive"
});
```

---

## Architecture Overview

```
Tick() ทุก frame:
  1. FWindGrid::DecayToAmbient()          ← grid ค่อยๆ decay กลับ ambient
  2. flecs::world::progress()
     ├── WindMotorInjectionSystem         ← motors inject velocity เข้า grid
     ├── WindSamplingSystem               ← sample grid ที่ตำแหน่ง foliage
     └── FoliageResponseSystem            ← damped oscillator → เขียน CPD
  3. MarkRenderStateDirty(dirty HISMs)    ← แจ้ง render thread
```

---

*Built with Flecs v3.2.12 · Unreal Engine 5.7 · Windows 64-bit*
