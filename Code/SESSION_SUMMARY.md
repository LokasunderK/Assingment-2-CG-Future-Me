# สรุปประวัติและรายละเอียดการทำงาน (Session Summary)
**โปรเจกต์:** Computer Graphics Assignment 2 - Theme "FUTURE ME"  
**วันที่บันทึก:** 16 ตุลาคม 2026  
**พื้นที่ทำงาน:** `c:\Storage\Y3T1\CG 1_2\Project\Code\`

---

## 1. ข้อกำหนดของโจทย์ (Assignment Requirements)
1. สร้างโปรแกรม Modern OpenGL เรนเดอร์ภาพขนาด **800 x 600 พิกเซล**
2. ธีมผลงาน: **"FUTURE ME"** (ภาพสะท้อนตัวฉันในอนาคต)
3. ใช้ระบบแสงและเงา (Lighting Model + Shadow Mapping)
4. อนุญาตให้นำ External 3D Models และ Textures ภายนอกมาใช้งานได้
5. การส่งงาน:
   - ซอร์สโค้ดภาษา C++ (เช่น `Assignment2_studentID.cpp` หรือโฟลเดอร์โปรเจกต์ที่พร้อมคอมไพล์)
   - ไฟล์ Shaders ที่จำเป็น
   - ภาพผลลัพธ์ที่แคปจากโปรแกรม ตั้งชื่อตามรูปแบบ: `Assignment2_studentID_yourPairID.png` หรือ `.jpg`

---

## 2. ลำดับเหตุการณ์และปัญหาที่พบระหว่าง Session (Timeline & Issue Log)

### เฟสที่ 1: การนำเข้าโมเดลจาก Blender สู่ OpenGL
- ผู้ใช้ได้ทำการจัดวางองค์ประกอบฉากต่างๆ ใน Blender และบันทึกเป็น `CgProject.blend`
- องค์ประกอบในฉากประกอบด้วย:
  - โครงกระดูก Minecraft นั่งก้มหน้าเศร้า
  - กล่องกระดาษลังที่โครงกระดูกนั่งทับอยู่
  - เนคไทติดที่ตัวโครงกระดูก
  - ตึกบริษัทขนาดใหญ่พร้อมป้ายโลโก้ 3D "Microsoft" ด้านหลัง
- **ปัญหาเริ่มต้น:** โค้ดแลปเดิมโหลดไฟล์ OBJ เป็นวัตถุชิ้นเดียว (Single Mesh) และไม่ได้โหลด Texture ตาม Material

### เฟสที่ 2: ปัญหาผลลัพธ์ผิดเพี้ยนรอบแรก
เมื่อทดสอบรันครั้งแรก ผู้ใช้พบปัญหาสำคัญ 4 ประการ:
1. **ลายกระดาษลังติดไปทั่วทั้งฉาก:** แม้แต่ตัวตึกก็กลายเป็นลายกระดาษลัง
2. **เนคไทขนาดใหญ่ยักษ์และผิดตำแหน่ง:** มีวัตถุเนคไทขนาดมหึมาลอยบังหน้าจอ
3. **พื้นฉากเป็นสีดำ:** ผู้ใช้ต้องการให้พื้นเป็น **สีเทา**
4. **ตัวโครงกระดูกมืดดำ:** ไม่เห็นลายกะโหลก ซี่โครง หรือลูกตา

### เฟสที่ 3: การวิเคราะห์เชิงลึก (Deep Inspection)
ได้เข้าไปตรวจสอบโฟลเดอร์ `C:\Storage\Y3T1\CG 1_2\Project\Model` และไฟล์ `.blend` ด้วยสคริปต์ Python Headless:
1. **สาเหตุเนคไทยักษ์:** ในโฟลเดอร์ Model มีทั้ง `black_tie.glb` และ `necktie.glb` ซึ่งในไฟล์ Blender มีวัตถุเนคไทเก่าชื่อ `Object_2` / `Object_3` (จาก `black_tie.glb`) ขนาดกว้าง 147 เมตร สูง 377 เมตรค้างอยู่ ส่วนเนคไทจริงที่ผู้ใช้วางไว้บนอกโครงกระดูกคือ `Object_2.001` (จาก `necktie.glb`)
2. **สาเหตุลายกระดาษลัง:** `main.cpp` โหลดเฉพาะ `Cardboard-Color.png` เข้าไปใน Texture Unit 0 แล้วสั่งวาดทั้งโมเดลด้วย Texture นี้
3. **สาเหตุโครงกระดูกมืด:** Texture ของโครงกระดูก Minecraft เป็นไฟล์ PNG แบบมี Alpha Channel (ความโปร่งใส) และแกนพิกัด Texture (UV) ใน OpenGL กลับหัวในแนวตั้ง (V-flip) ทำให้เมื่ออ่านค่าพิกเซลไปตกตรงจุดที่มี Alpha = 0 และสีดำ

### เฟสที่ 4: การแก้ไขปัญหาและยกระดับสถาปัตยกรรมโค้ด
1. **ลบเนคไทยักษ์ 377 เมตร:** ทำความสะอาดไฟล์โมเดลใน Blender และ Export ออกมาเป็น `.obj` และ `.mtl` ตัวใหม่ที่สะอาดและขนาดไฟล์ลดลงเหลือเพียง 1.55 MB
2. **อัปเกรด `Mesh.h` และ `Mesh.cpp` รองรับ Multi-Material:**
   - เพิ่มโครงสร้าง `SubMesh` และ `Material`
   - ตัวโหลด OBJ จะอ่านแท็ก `usemtl` และ `mtllib` เพื่อจัดกลุ่ม Index Buffer แยกตาม Material
   - ทำการเรนเดอร์แยก 1 Draw Call ต่อ 1 Material พร้อมสลับ Texture และสี Diffuse ให้อัตโนมัติ
3. **จับคู่ Texture และสีอย่างถูกต้อง:**
   - `Cardboard`: ใช้ `Cardboard-Color.png` (ลายกระดาษลังติดเฉพาะที่กล่องเท่านั้น)
   - `material_0`: โครงกระดูก Minecraft ใช้ `Image_0.002.png`
   - `Material.001` / `Material.002`: ตัวตึกใช้อิฐและกระจก `Image_0.001.png`
   - `Material.005`: ปรับสีพื้นเป็น **สีเทากลาง** (`glm::vec3(0.45f)`)
   - `Mat_1`: เนคไทบนตัวโครงกระดูกแสดงเป็นสีแดงเด่นชัด (`glm::vec3(0.85f, 0.15f, 0.15f)`)
   - `67, 69, 71, 73, 65`: สีโลโก้ Microsoft (แดง, เขียว, น้ำเงิน, เหลือง และตัวอักษร 3D สีเทา)
4. **แก้ไข UV V-flip และ Alpha ใน Shader:**
   - เพิ่ม `stbi_set_flip_vertically_on_load(true);` ทำให้ลายกะโหลกและซี่โครงตรงกับโมเดล
   - ใน `shader.frag` เพิ่มโค้ดดักจับ Alpha ถ้าพิกเซลโปร่งใสให้ใช้สีพื้นฐานของกระดูกแทนสีดำ
5. **ปรับมุมกล้อง (Composition):**
   - จัดมุมกล้องให้เห็นเรื่องราว "FUTURE ME" อย่างชัดเจน: โครงกระดูกใส่เนคไทนั่งบนกล่องกระดาษลังในระยะหน้า (Foreground) และมองเห็นตึกสำนักงานพร้อมป้าย Microsoft ในระยะหลัง (Background)

### เฟสที่ 5: การรวมไฟล์ Shaders ให้กระชับ
- ผู้ใช้สอบถามว่า `depthShader.vert` / `depthShader.frag` สามารถรวมเข้ากับ `shader.vert` / `shader.frag` ได้หรือไม่
- ได้ทำการรวม Shaders ทั้งหมดเหลือเพียง **คู่เดียว**:
  - เพิ่มตัวแปร `uniform bool isDepthPass;`
  - เมื่อเป็น Depth Pass (ทำเงา): คำนวณเฉพาะพิกัดแสงและข้ามการคำนวณสีใน Fragment Shader
  - เมื่อเป็น Lit Pass (วาดจริง): คำนวณแสงเงา Blinn-Phong + Texture ตามปกติ
  - ลบไฟล์ `depthShader.vert` และ `depthShader.frag` ออกจากโปรเจกต์

### เฟสที่ 6: การจัดระเบียบโฟลเดอร์และเปลี่ยนชื่อเป็น `ModelFutureMe`
- ลบไฟล์ที่อยู่นอกโฟลเดอร์ที่ไม่ได้ใช้งาน: `Assignment2_preview.png`, `CgProject.blend`, `CgProject.obj`, `CgProject.mtl`
- เปลี่ยนชื่อโมเดลหลักทั้งหมดจาก `CgProject` เป็น **`ModelFutureMe`**:
  - `OpenGLStarter/Models/ModelFutureMe.obj`
  - `OpenGLStarter/Models/ModelFutureMe.mtl`
  - `Model/ModelFutureMe.blend`
  - อัปเดต `main.cpp` ให้เรียก `modelDirectory / "ModelFutureMe.obj"`
- ปรับระบบแคปภาพหน้าจอ: บันทึกภาพลงไฟล์โดยตรงเมื่อกดปุ่ม **`P`**

### เฟสที่ 7: ตรวจสอบและแก้บั๊ก (Review & Fixes)
1. **สี fallback ของ texel โปร่งใสค้างจาก submesh ก่อนหน้า:** `Mesh::RenderWithMaterials` ส่ง `objectColour` (Kd) ทุก submesh แล้ว ไม่ใช่เฉพาะ material ที่ไม่มี texture และ unbind shadow map ที่ texture unit 1 ให้ถูกต้องหลัง Lit Pass
2. **Texture โครงกระดูกเบลอ:** texture ขนาด ≤128px (เช่น `Image_0.002.png` 64x64) ใช้ `GL_NEAREST` ให้ดูเป็นพิกเซลแบบ Minecraft
3. **ขนาดภาพส่งงาน:** ล็อกหน้าต่างไม่ให้ resize (`GLFW_RESIZABLE = false`) ภาพจึงเป็น 800x600 เสมอ, ตั้งชื่อหน้าต่าง "FUTURE ME", ชื่อไฟล์ภาพตามรูปแบบ `Assignment2_studentID_yourPairID.png`, กด P ค้างไม่เซฟซ้ำทุกเฟรม
4. **มุมกล้องใหม่:** มุมต่ำมองเงยจาก `(-4.9, 0.9, -13.2)` yaw 68.5° pitch 13.5° เห็นโครงกระดูกทั้งตัว ตึก และโลโก้ Microsoft ครบไม่โดนขอบตัด (เสาไฟ Object_9/10 อยู่นอกเฟรม)
5. **Specular เป็น Blinn-Phong จริง:** เดิมใช้ `reflect()` แบบ Phong เปลี่ยนเป็น half vector (`normalize(lightDir + viewDir)`) shininess 64

---

## 3. โครงสร้างไฟล์ในโปรเจกต์ปัจจุบัน (Current File Structure)

```text
Project/
├── Model/                                 <- โฟลเดอร์เก็บ Asset ต้นทาง
│   ├── ModelFutureMe.blend               <- ไฟล์ Blender ต้นฉบับที่จัดฉากเสร็จสมบูรณ์
│   ├── building.glb, minecraft_sad_skeleton_sitting.glb, necktie.glb, black_tie.glb
│   ├── microsoft-logo-2012-present/      <- โลโก้ Microsoft (glb + textures)
│   └── worn-and-flattened-cardboard/     <- กล่องกระดาษ (fbx + textures)
│
└── Code/
    ├── SESSION_SUMMARY.md                <- เอกสารสรุปประวัติ Session นี้
    └── OpenGLStarter/
        ├── CMakeLists.txt
        ├── Shaders/
        │   ├── shader.vert               <- Unified Vertex Shader (รองรับทั้ง Depth & Lit pass)
        │   └── shader.frag               <- Unified Fragment Shader (Blinn-Phong + Shadow Mapping)
        ├── Models/
        │   ├── ModelFutureMe.obj         <- โมเดลหลักที่โปรแกรมโหลด (1.55 MB)
        │   └── ModelFutureMe.mtl         <- การตั้งค่า Material และจับคู่ Texture
        ├── Textures/
        │   ├── Cardboard-Color.png       <- Texture กล่องกระดาษลัง (ใช้งาน)
        │   ├── Image_0.001.png           <- Texture อิฐและหน้าต่างตึก (ใช้งาน)
        │   ├── Image_0.002.png           <- Texture โครงกระดูก Minecraft (ใช้งาน)
        │   └── (Textures เสริมอื่นๆ)
        └── src/
            ├── main.cpp                  <- โค้ดหลักของโปรแกรม
            ├── ProjectPaths.h.in
            └── Libs/
                ├── Mesh.h / Mesh.cpp     <- ระบบ Multi-Material Mesh & OBJ Loader
                ├── Shader.h / Shader.cpp <- ระบบโหลดและคอมไพล์ GLSL
                └── Window.h / Window.cpp <- ระบบจัดการหน้าต่าง GLFW & Input
```

---

## 4. รายการไฟล์และโค้ดที่ไม่ได้ใช้งาน (Unused Resources)

| ชนิด | รายการ | คำอธิบาย |
| :--- | :--- | :--- |
| **Texture ใน `Textures/`** | `Image_2.png` (~13.9 MB) | เป็น Roughness Map ของตึกจาก Blender ที่ shader ไม่ได้นำมาคำนวณ |
| **Texture ใน `Textures/`** | `Image_1.002.png` (~1.07 MB) | เป็น Metallic Map ของตึกที่ไม่ได้ใช้งาน |
| **Texture ใน `Textures/`** | `CardboardNormal.jpg` (~359 KB) | เป็น Normal Map ของกล่องกระดาษ (ปัจจุบันใช้ Flat Normal + Shadow) |
| **Texture ใน `Textures/`** | `Image_0.003.png` (~122 KB) | Texture จากเนคไทยักษ์ตัวเก่าที่ถูกลบออกไปแล้ว |
| **โมเดลใน `Model/`** | `black_tie.glb` (~546 KB) | โมเดลเนคไทขนาด 377 เมตรที่ผิดสเกล |
| **โค้ดใน `shader.frag`** | `uniform bool isLight;` | ตัวแปรและบล็อกเงื่อนไขวาดลูกบาศก์แสงจากแลปเก่า |
| **โค้ดใน `Mesh.cpp`** | `CreateMesh(const GLfloat*...)` | ฟังก์ชันสร้าง Mesh แบบรับ float array ดิบจากแลปเก่า |

---

## 5. วิธีการรันและการบันทึกภาพส่งงาน (How to Run & Submit)

### การคอมไพล์และรัน:
สามารถคอมไพล์และรันผ่าน CMake ใน Terminal หรือ VS Code / Visual Studio:
```powershell
# คอมไพล์ผ่าน CMake
cmake --build OpenGLStarter/build/win-msvc-2026 --config Debug

# รันโปรแกรม
& "OpenGLStarter\build\win-msvc-2026\Debug\OpenGLStarter.exe"
```

### การควบคุมในโปรแกรม:
- **คลิกเมาส์ซ้ายค้างแล้วเลื่อน:** หมุนมุมมองกล้อง (Look Around)
- **W / A / S / D:** เคลื่อนที่ไป ข้างหน้า / ซ้าย / ถอยหลัง / ขวา
- **Spacebar / Left Shift:** บินขึ้น / บินลง
- **Scroll Wheel เมาส์:** ซูมเข้า / ซูมออก (Field of View)
- **กดปุ่ม `R`:** รีเซ็ตมุมมองกล้องกลับมาที่มุมมาตรฐาน (มองเห็นโครงกระดูกและตึกอย่างสมบูรณ์)
- **กดปุ่ม `P`:** บันทึกภาพหน้าจอ 1 ครั้งต่อการกด ขนาด 800x600 พิกเซล เซฟเป็น **`Assignment2_<STUDENT_ID>_<PAIR_ID>.png`** ในโฟลเดอร์ที่รันโปรแกรม (แก้ค่า `STUDENT_ID` / `PAIR_ID` ที่ต้น `main.cpp` ก่อนส่ง)
- **รันด้วย `--screenshot-and-exit`:** แคปภาพที่เฟรมที่ 5 แล้วปิดโปรแกรมเอง

---
*เอกสารสรุปนี้ถูกสร้างขึ้นเพื่อเป็นบันทึกสรุปงานและกระบวนการพัฒนาสำหรับ Assignment 2*
