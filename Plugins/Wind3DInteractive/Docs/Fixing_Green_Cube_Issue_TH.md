# บันทึกการแก้ไข (Bug Fix): ปัญหาลูกบาศก์สีเขียว (Green Cube Issue)

## ปัญหาที่พบ (The Issue)
- **อาการ**: ลูกบาศก์แสดงผลเป็น **สีเขียวทึบ (Solid Green)** และไม่มีการเคลื่อนไหวของกระแสลม แม้ว่าระบบ Wind Simulation จะทำงานอยู่ (ตรวจสอบจาก 2D Debugger แล้วทำงานปกติ)
- **ความถี่**: มักเกิดขึ้นเมื่อเริ่มเล่น (BeginPlay) หรือเมื่อมีการเปลี่ยนขนาดของ Wind Grid

## สาเหตุ (The Cause)
ปัญหาเกิดจาก **"Stale Texture Reference"** (การอ้างอิง Texture ที่หมดอายุแล้ว) ใน `WindTextureManager`:

1. เมื่อระบบ Wind Grid เริ่มต้นใหม่ (Initialize) หรือเปลี่ยนขนาด มันจะทำการ **ทำลาย (Destroy)** `UVolumeTexture` ตัวเก่า และสร้างตัวใหม่ขึ้นมา
2. แต่ **Material Instance Dynamic (MID)** ที่ถูกสร้างและแปะไว้บนลูกบาศก์ ยังคงจำ **Pointer ของ Texture ตัวเก่า** ที่ถูกทำลายไปแล้ว
3. ผลลัพธ์คือ Material อ่านค่าจาก Texture ที่ไม่ถูกต้อง (Invalid/Null) ทำให้แสดงผลเป็นสีเขียว (ค่า Default หรือกระแสลมเป็น 0)

## การแก้ไข (The Fix)
แก้ไขที่ไฟล์ `WindTextureManager.cpp` ในฟังก์ชัน `UpdateBoundMIDs`:

เราเพิ่มโค้ดเพื่อ **บังคับอัปเดต (Force Update)** ค่า Parameter `WindVolume` ใหม่ทุกครั้งที่มีการประมวลผล Grid เพื่อให้แน่ใจว่า Material จะได้รับ Texture ตัวล่าสุดเสมอ

```cpp
// ในไฟล์ WindTextureManager.cpp

void FWindTextureManager::UpdateBoundMIDs(const IWindSolver& Grid)
{
    // ... (อัปเดตค่า Origin และ Size ตามปกติ) ...

    // [FIX] บังคับส่ง Texture ใหม่เข้าไปใน Material Instance ทุกเฟรม
    // เพื่อป้องกันกรณีที่ Texture เก่าถูกทำลายและสร้างใหม่
    if (WindVolumeTexture)
    {
        MID->SetTextureParameterValue(FName(TEXT("WindVolume")), WindVolumeTexture);
    }
}
```

และในส่วนของ Material Builder (`WindMaterialBuilder.cpp`) เราได้ยกเลิกการใช้ `MaterialParameterCollection` (MPC) ชั่วคราว และกลับมาใช้ **Vector Parameter** ธรรมดา เพื่อความเสถียรและควบคุมได้ง่ายกว่า

## วิธีการตรวจสอบ (Verification)
1. **Recompile** โปรเจกต์ (ปิด Editor แล้ว Build จาก IDE)
2. เปิด Editor และรันคำสั่ง: `Wind3D.CreateDebugMaterial /Game/Wind/M_WindVolume3D_Fixed`
3. กด Play
4. ลูกบาศก์ควรแสดงสี ไล่ระดับ แดง/เหลือง/เขียว ตามความแรงลม แทนที่จะเป็นสีเขียวทึบตลอดเวลา
