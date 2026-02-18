nity replicates the 3DRT wind field system from God of War 4.
SabreGodLY
SabreGodLY
Looking for a new job
151 people liked the article.
​
Table of contents
Collapse
Foreword
Prepare
Start implementing
ComputeShader
Create Buffer
Achievement of the effect
Offset of wind data
Wind diffusion simulation
Wind speed calculation and storage for wind turbines
Simulation of wind advection and convection
The final wind speed information from each channel is stored in a single 3D RT image.
Performance on mobile devices
Finally, add a little something extra.
refer to
Foreword
I recently saw a presentation on God of War 4 at GDC19 , and the 3D wind field concept was quite interesting, so I thought I'd recreate it in Unity.

I strongly recommend watching the GDC presentation video and the slides below before reading this article, as this article focuses on the specific implementation and will not repeat the content presented at GDC.


Si Huhu: Wind Simulation in 'God of War' (GDC2019) - God of War Wind Field
205 Likes · 11 Commentsarticle

(PS: I later created a wind simulation version using the Unity ECS system , so you can confidently achieve the same effect on mobile devices. The overall idea is to distribute the logic calculations that would otherwise require Compute Shaders to the Job System and Burst , and optimize for potential sync points. This article mainly introduces the implementation of wind simulation; for more details, I recommend reading the repository source code at the end of the article.)

Prepare
This implementation uses Unity 2021 LTS version, and URP version 12.1.6.

The URP version doesn't actually have a big impact; it just uses URP's RenderFeature to trigger the rendering of the wind turbine. But why use such a new Unity version?

It was initially developed based on version 2019.4.16, but the older version had a very annoying problem—

We know that URP's CommandBuffer is stateless, meaning that after each frame's rendering task is completed through the context, the CommandBuffer starting in the next frame is unaware of which rendering data (RT) data was held in the previous frame. However, our 3D rendering data for wind field simulation needs to be reused, and there are relationships between frames. Therefore, we need to create an external class to manage this part of the RT declarations and ComputeShader instructions.

However, what's puzzling is that in the 2019 version of Unity, after creating the RT and passing it in the ComputeShader, and declaring the global shader, it still receives an empty RT. But if you check the memory data of the Profiler , you can see that the RT has actually been declared.

What's really interesting is that if you double-click to view this RT, this RT can be passed to ComputeShader and Shader before this Unity restart.

The code remained unchanged, and after upgrading to the 2021 LTS version, there were no issues whatsoever. I'm certain this problem is a Unity bug (just rambling).

Start implementing
ComputeShader
The wind data storage method used in God of War is to use two 32x16x32 3D RenderTextureBuffers with three channels each , plus one 3D RenderTextureBuffer that combines all channels . The reason for this will be explained later.

Before proceeding, let's briefly introduce ComputeShader.


Figure 1. A basic ComputeShader created in Unity.

Figure 2 shows a portion of the source code, specifically the statement in the C# side that calls Dispatch to execute the ComputeShader.

Figure 3 shows the setting for the number of thread groups (Dispatch) and the setting for the number of threads in each thread group (numthread).
For a newly created CS (Client/Server) application, it's necessary to understand several basic concepts.

Kernel: The computation kernel represents the computation functions you register in this client-side application (CS), which is the entry point for CS functionality . Multiple kernels can exist in scripts like C#.
`numthread`: The size of a single thread group, indicating the number of threads within a thread group . For example, Figure 1 shows a thread group with 8 * 8 * 1 = 64 threads , while Figure 3 shows 10 * 8 * 3 = 240 threads.
dispatch: Thread group array, indicating how many thread groups will be created when this ComputeShader is called in C#.
In the diagram, m_WindBrandX, m_WindBrandY, and m_WindBrandZ are 32, 16, and 32 respectively. Therefore, x, y, and z in the Dispatch function above are 8, 4, and 8 respectively, totaling 256 thread groups.

(However, in God of War 4, for the sake of parallel efficiency of ComputeShader, the y-axis and z-axis were swapped, and the z-axis was kept at 1 when setting numthreads in ComputeShader, which is compatible with some devices with lower CS versions. This step is skipped here just to achieve the desired effect.)

Create Buffer
(At first, when I was recreating it, I foolishly used Texture3D...)

What we actually need to create is a RenderTexture, and set its dimensions to 3D. The width and height still represent the size in the x and y dimensions, and the depth is the size in the z dimension.

In the presentation, the "God of War" mentioned that breaking down the channels for processing can improve running speed and allow for higher precision to meet the needs of subsequent atomic calculations. Therefore, six 3DRenderTextures (RTs) need to be created, along with one final RT for the shader to sample. The RT format for the channels was also mentioned in the PPT, using a signed 32-bit integer SInt. The advantage of using this channel is that it allows for positive and negative values, eliminating the need for mapping as in NormalMap. The final RT doesn't need to distinguish between channels anymore; it uses RGBA32, meaning the calculation results from the previously split channels are merged into this RT. Although this format seems daunting, the 3DRenderTexture is very small, so it should be manageable (I think).


Figure 4 shows the RT declaration, including 6 single-channel RT buffers and 1 RT buffer ultimately used for sampling.

Figure 5 RT format and RT creation
Achievement of the effect
The following section will introduce the implementation process of the 3D wind field in God of War 4.

Based on the process of achieving the desired effect, the process can be divided into the following parts.

Offset storage of wind data
Wind diffusion simulation
Wind speed calculation and storage for wind turbines
Simulation of wind advection and convection
The final wind speed information from each channel is stored in a single 3D RT image.
As for why we didn't start with calculations for wind turbines? I don't care, just listen to me.

Offset of wind data
The wind force calculation uses a single-channel RT storage data format, which is only merged into a mixed RGB channel 3DRenderTexture during output.

When starting the first step of the calculation, it is necessary to have a concept that the wind force data of the previous frame will be carried over to the next frame and participate in the simulation calculation and effect representation.

This step is to correct the recording deviation of wind information caused by the movement of the wind center object.

Assuming the central object O of our wind force recording is the player we are currently controlling, and the world space coordinates of O in the current frame are denoted as...
In this frame, our wind speed information is calculated and stored in this coordinate system.

Suppose that in the next frame, O moves 1 meter further in the positive x-axis direction (in RT, 1 pixel represents 1 meter of space data), shouldn't the center point of the wind record in the next frame be...
So what? Therefore, for the next frame, all wind information must be shifted 1 pixel in the opposite direction of the x-axis in order to accurately inherit the wind information in each world space.

Based on this pattern, we can implement the following offset algorithm and corresponding ComputeShader.

Note that in RT, the storage rule is that 1 cubic centimeter represents one pixel, so all operations on RT need to be converted to integers.


The reason for including the `from` directive in Figure 6 is for ease of debugging; once the desired effect is achieved, it's optional.

(PS: I encountered a really annoying bug here. Looking at the code, you can see that I'm passing the offsets of each channel separately into the ComputeShader. The reason for this is that whether I use an int[] array or a Vector3 to Int converter, the data cannot be stored... If anyone knows the reason, please let me know...)

Wind diffusion simulation
The presenter in the video mentioned it himself, and it was also mentioned in the PPT. For diffusion simulation, you only need to think of this effect as a blur.

Let's do it in a vague way then.


Figure 7
#include "WindSimulationInclude.hlsl"
#pragma kernel CSMain

Texture3D<int> WindBufferInput;
RWTexture3D<int> WindBufferOutput;

uniform int3 VolumeSizeMinusOne;
uniform float DiffusionForce;

#define N 4
groupshared float m_Cache[N*N*N];

[numthreads(N,N,N)]
void CSMain (int3 dispatchThreadID : SV_DispatchThreadID, int3 groupThreadID : SV_GroupThreadID)
{
    // 采样int格式的Volume纹理,并转换格式
    float windInput = PackIntToFloat(WindBufferInput[dispatchThreadID.xyz].r);
    // 计算节点Groupid
    int cacheIndex = groupThreadID.x + groupThreadID.y * 4 + groupThreadID.z * 16;
    m_Cache[cacheIndex] = windInput;
    GroupMemoryBarrierWithGroupSync();
    // 判断各个方向上的越界问题和数据cache获取逻辑
    float xr = 0;
    float xl = 0;
    float yr = 0;
    float yl = 0;
    float zr = 0;
    float zl = 0;
    // X轴
    if(groupThreadID.x < N - 1)
    {
        int3 gtID = groupThreadID + int3(1, 0, 0);
        xr = m_Cache[gtID.x + gtID.y * 4 + gtID.z * 16];
    }
    else
    {
        int tIDx = min(dispatchThreadID.x + 1, VolumeSizeMinusOne.x);
        xr = PackIntToFloat(WindBufferInput[int3(tIDx, dispatchThreadID.y, dispatchThreadID.z)].r);
    }
    if(groupThreadID.x != 0)
    {
        int3 gtID = groupThreadID + int3(-1, 0, 0);
        xl = m_Cache[gtID.x + gtID.y * 4 + gtID.z * 16];
    }
    else
    {
        int tIDx = max(dispatchThreadID.x - 1, 0);
        xl = PackIntToFloat(WindBufferInput[int3(tIDx, dispatchThreadID.y, dispatchThreadID.z)].r);
    }
    // Y轴Z轴同理，省略
    ......
    // 最终合并diffusion模拟
    float finalData = xr + xl + yr + yl + zr + zl - windInput * 6;
    finalData = finalData * DiffusionForce + windInput;
    WindBufferOutput[dispatchThreadID.xyz] = PackFloatToInt(finalData);
}
PackIntToFloat and PackFloatToInt are methods I encapsulated myself; they essentially just implement numerical transformations in shared PowerPoint presentations.


Figure 8 shows the part within the red box, and then a function was created to restore it to float.
Wind speed calculation and storage for wind turbines
This is the core part of the simulation, requiring us to record the wind turbines in the scene and store the wind speed information into a single-channel RT buffer.

The God of War 4 presentation slides directly cover the algorithms for three wind turbine engines, which can be used for simulation and replication.


Figure 9 shows the algorithms for engine types mentioned in the official sharing.
We need to write the engine WindMotor in C# and hold and manage all the motor's lifecycle and data updates in our simulation script.

We need to use the previously calculated fuzzy data as a basis, and then superimpose it on the calculated values ​​of the wind turbine.

// 风力发动机类代码（部分）
public enum MotorType
{
    Directional,
    Omni,
    Vortex,
}

public struct MotorDirectional
{
    public Vector3 position;
    public float radiusSq;
    public Vector3 force;
}

public struct MotorOmni
{
    public Vector3 position;
    public float radiusSq;
    public float force;
}

public struct MotorVortex
{
    public Vector3 position;
    public Vector3 axis;
    public float radiusSq;
    public float force;
}

public class WindMotor : MonoBehaviour
{
    public MotorType MotorType;
    public MotorDirectional motorDirectional;
    public MotorOmni motorOmni;
    public MotorVortex motorVortex;

    private static MotorDirectional emptyMotorDirectional = new MotorDirectional();
    private static MotorOmni emptyMotorOmni = new MotorOmni();
    private static MotorVortex emptyMotorVortex = new MotorVortex();

    public static MotorDirectional GetEmptyMotorDirectional()
    {
        return emptyMotorDirectional;
    }
    public static MotorOmni GetEmptyMotorOmni()
    {
        return emptyMotorOmni;
    }
    public static MotorVortex GetEmptyMotorVortex()
    {
        return emptyMotorVortex;
    }
    
    /// <summary>
    /// 创建风力发电机的时间，以Time.fixedTime为准
    /// </summary>
    private float m_CreateTime;
    public bool Loop = true;
    public float LifeTime = 5f;
    [Range(0.001f, 100f)]
    public float Radius = 1f;
    public AnimationCurve RadiusCurve = AnimationCurve.Linear(1, 1, 1, 1);
    public Vector3 Asix = Vector3.up;
    public float Force = 1f;
    public AnimationCurve ForceCurve = AnimationCurve.Linear(1, 1, 1, 1);
    public float Duration = 0f;

    private Vector3 m_prePosition = Vector3.zero;
    #region BasicFunction

    private void Start()
    {
        m_CreateTime = Time.fixedTime;
    }

    private void OnEnable()
    {
        WindManager.Instance.AddWindMotor(this);
        m_CreateTime = Time.fixedTime;
    }

    private void OnDisable()
    {
        WindManager.Instance.RemoveWindMotor(this);
    }

    private void OnDestroy()
    {
        WindManager.Instance.RemoveWindMotor(this);
    }
    #endregion
    
    #region MainFunction
    /// <summary>
    /// 检查声明周期结束
    /// </summary>
    /// <param name="duration"></param>
    void CheckMotorDead()
    {
        float duration = Time.fixedTime - m_CreateTime;
        if (duration > LifeTime)
        {
            if (Loop)
            {
                m_CreateTime = Time.fixedTime;
            }
            else
            {
                m_CreateTime = 0f;
                WindPool.Instance.PushWindMotor(this.gameObject);
            }
        }
    }
    #endregion
    
    #region UpdateForceAndOtherProperties
    /// <summary>
    /// 调用的时候才更新风的参数
    /// </summary>
    public void UpdateWindMotor()
    {
        switch (MotorType)
        {
            case MotorType.Directional:
                UpdateDirectionalWind();
                break;
            case MotorType.Omni:
                UpdateOmniWind();
                break;
            case MotorType.Vortex:
                UpdateVortexWind();
                break;
        }
    }
    private void UpdateDirectionalWind()
    {
        float duration = Time.fixedTime - m_CreateTime;
        float timePerc = duration / LifeTime;
        Duration = timePerc;
        float radius = Radius * RadiusCurve.Evaluate(timePerc);
        motorDirectional = new MotorDirectional()
        {
            position = transform.position,
            radiusSq = radius * radius,
            force = transform.forward * ForceCurve.Evaluate(timePerc) * Force
        };
        CheckMotorDead();
    }

    private void UpdateOmniWind()
    {
        float duration = Time.fixedTime - m_CreateTime;
        float timePerc = duration / LifeTime;
        Duration = timePerc;
        float radius = Radius * RadiusCurve.Evaluate(timePerc);
        motorOmni = new MotorOmni()
        {
            position = transform.position,
            radiusSq = radius * radius,
            force = ForceCurve.Evaluate(timePerc) * Force
        };
        CheckMotorDead();
    }

    private void UpdateVortexWind()
    {
        float duration = Time.fixedTime - m_CreateTime;
        float timePerc = duration / LifeTime;
        Duration = timePerc;
        float radius = Radius * RadiusCurve.Evaluate(timePerc);
        motorVortex = new MotorVortex()
        {
            position = transform.position,
            axis = Vector3.Normalize(Asix),
            radiusSq = radius * radius,
            force = ForceCurve.Evaluate(timePerc) * Force
        };
        CheckMotorDead();
    }
    #endregion
}

//风力发动机管理逻辑和ComputeShader数据传输逻辑（部分）
void DoRenderWindVelocityData(int form)
    {
        if (MotorsSpeedCS != null && BufferExchangeCS != null)
        {
            m_DirectionalMotorList.Clear();
            m_OmniMotorList.Clear();
            m_VortexMotorList.Clear();

            int directionalMotorCount = 0;
            int omniMotorCount = 0;
            int vortexMotorCount = 0;
            foreach (WindMotor motor in m_MotorList)
            {
                // 更新风力发动机数据
                motor.UpdateWindMotor();
                switch (motor.MotorType)
                {
                    case MotorType.Directional:
                        if (directionalMotorCount < MAXMOTOR)
                        {
                            m_DirectionalMotorList.Add(motor.motorDirectional);
                            directionalMotorCount++;
                        }
                        break;
                    case MotorType.Omni:
                        if (omniMotorCount < MAXMOTOR)
                        {
                            m_OmniMotorList.Add(motor.motorOmni);
                            omniMotorCount++;
                        }
                        break;
                    case MotorType.Vortex:
                        if (vortexMotorCount < MAXMOTOR)
                        {
                            m_VortexMotorList.Add(motor.motorVortex);
                            vortexMotorCount++;
                        }
                        break;
                }
            }
            // 往列表数据中插入空的发动机数据
            if (directionalMotorCount < MAXMOTOR)
            {
                MotorDirectional motor = WindMotor.GetEmptyMotorDirectional();
                for (int i = directionalMotorCount; i < MAXMOTOR; i++)
                {
                    m_DirectionalMotorList.Add(motor);
                }
            }
            if (omniMotorCount < MAXMOTOR)
            {
                MotorOmni motor = WindMotor.GetEmptyMotorOmni();
                for (int i = omniMotorCount; i < MAXMOTOR; i++)
                {
                    m_OmniMotorList.Add(motor);
                }
            }
            if (vortexMotorCount < MAXMOTOR)
            {
                MotorVortex motor = WindMotor.GetEmptyMotorVortex();
                for (int i = vortexMotorCount; i < MAXMOTOR; i++)
                {
                    m_VortexMotorList.Add(motor);
                }
            }

            m_DirectionalMotorBuffer.SetData(m_DirectionalMotorList);
            MotorsSpeedCS.SetBuffer(m_MotorSpeedKernel, m_DirectionalMotorBufferId, m_DirectionalMotorBuffer);
            m_OmniMotorBuffer.SetData(m_OmniMotorList);
            MotorsSpeedCS.SetBuffer(m_MotorSpeedKernel, m_OmniMotorBufferId, m_OmniMotorBuffer);
            m_VortexMotorBuffer.SetData(m_VortexMotorList);
            MotorsSpeedCS.SetBuffer(m_MotorSpeedKernel, m_VortexMotorBufferId, m_VortexMotorBuffer);

            MotorsSpeedCS.SetFloat(m_DirectionalMotorBufferCountId, directionalMotorCount);
            MotorsSpeedCS.SetFloat(m_OmniMotorBufferCountId, omniMotorCount);
            MotorsSpeedCS.SetFloat(m_VortexMotorBufferCountId, vortexMotorCount);
            MotorsSpeedCS.SetVector(m_VolumePosOffsetId, m_OffsetPos);
            
            var formRTR = form == 1 ? m_WindBufferChannelR1 : m_WindBufferChannelR2;
            var formRTG = form == 1 ? m_WindBufferChannelG1 : m_WindBufferChannelG2;
            var formRTB = form == 1 ? m_WindBufferChannelB1 : m_WindBufferChannelB2;
            var toRTR = form == 1 ? m_WindBufferChannelR2 : m_WindBufferChannelR1;
            var toRTG = form == 1 ? m_WindBufferChannelG2 : m_WindBufferChannelG1;
            var toRTB = form == 1 ? m_WindBufferChannelB2 : m_WindBufferChannelB1;
            
            MotorsSpeedCS.SetTexture(m_MotorSpeedKernel, m_WindBufferInputXID, formRTR);
            MotorsSpeedCS.SetTexture(m_MotorSpeedKernel, m_WindBufferInputYID, formRTG);
            MotorsSpeedCS.SetTexture(m_MotorSpeedKernel, m_WindBufferInputZID, formRTB);
            MotorsSpeedCS.SetTexture(m_MotorSpeedKernel, m_WindBufferOutputXID, toRTR);
            MotorsSpeedCS.SetTexture(m_MotorSpeedKernel, m_WindBufferOutputYID, toRTG);
            MotorsSpeedCS.SetTexture(m_MotorSpeedKernel, m_WindBufferOutputZID, toRTB);
            MotorsSpeedCS.Dispatch(m_MotorSpeedKernel, m_WindBrandX / 8, m_WindBrandY / 8, m_WindBrandZ);
            // 清除旧Buffer
            BufferExchangeCS.SetTexture(m_ClearBufferKernel, m_WindBufferOutputXID, formRTR);
            BufferExchangeCS.SetTexture(m_ClearBufferKernel, m_WindBufferOutputYID, formRTG);
            BufferExchangeCS.SetTexture(m_ClearBufferKernel, m_WindBufferOutputZID, formRTB);
            BufferExchangeCS.Dispatch(m_ClearBufferKernel, m_WindBrandX / 4, m_WindBrandY / 4, m_WindBrandZ / 4);
        }
    }

// ComputeShader相关部分代码
#include "WindSimulationInclude.hlsl"
#pragma kernel WindVolumeRenderMotorCS

StructuredBuffer<MotorDirectional> DirectionalMotorBuffer;
StructuredBuffer<MotorOmni> OmniMotorBuffer;
StructuredBuffer<MotorVortex> VortexMotorBuffer;
uniform float DirectionalMotorBufferCount;
uniform float OmniMotorBufferCount;
uniform float VortexMotorBufferCount;
// 风力坐标计算的采样偏移
uniform float3 VolumePosOffset;



Texture3D<int> WindBufferInputX;
Texture3D<int> WindBufferInputY;
Texture3D<int> WindBufferInputZ;
RWTexture3D<int> WindBufferOutputX;
RWTexture3D<int> WindBufferOutputY;
RWTexture3D<int> WindBufferOutputZ;

// 根据传入的风力发动机的buffer，覆盖对应的扩散风的信息
[numthreads(8,8,1)]
void WindVolumeRenderMotorCS (uint3 id : SV_DispatchThreadID)
{
    float3 cellPosWS = id.xyz + VolumePosOffset;
    
    float3 velocityWS = 0;
    velocityWS.x = PackIntToFloat(WindBufferInputX[id.xyz].r);
    velocityWS.y = PackIntToFloat(WindBufferInputY[id.xyz].r);
    velocityWS.z = PackIntToFloat(WindBufferInputZ[id.xyz].r);
    // 根据已有的风力数量来叠加风力信息
    if(DirectionalMotorBufferCount > 0)
    {
        for(int i = 0; i < DirectionalMotorBufferCount; i++)
        {
            ApplyMotorDirectional(cellPosWS, DirectionalMotorBuffer[i], velocityWS);
        }
    }
    if(OmniMotorBufferCount > 0)
    {
        for(int i = 0; i < OmniMotorBufferCount; i++)
        {
            ApplyMotorOmni(cellPosWS, OmniMotorBuffer[i], velocityWS);
        }
    }
    if(VortexMotorBufferCount > 0)
    {
        for(int i = 0; i < VortexMotorBufferCount; i++)
        {
            ApplyMotorVortex(cellPosWS, VortexMotorBuffer[i], velocityWS);
        }
    }

    WindBufferOutputX[id.xyz] = PackFloatToInt(velocityWS.x);
    WindBufferOutputY[id.xyz] = PackFloatToInt(velocityWS.y);
    WindBufferOutputZ[id.xyz] = PackFloatToInt(velocityWS.z);
}
Simulation of wind advection and convection

Figure 10. Sharing PPT content on advection and convection.
The overall idea of ​​this step is to calculate the wind direction, then calculate the pixel distance to be moved in that direction based on the wind intensity, and finally move the speed information to the target pixel grid based on this moving distance.

For convection simulation, the approach taken by the author is simply to invert the coefficients of the advection simulation.

Regardless of what other people's intentions are, as long as the result is acceptable, that's fine.

#pragma kernel CSMain
#pragma kernel CSMain2
// 传入3个不同方向的风力纹理
Texture3D<int> WindBufferInputX;
Texture3D<int> WindBufferInputY;
Texture3D<int> WindBufferInputZ;
// 这个纹理代表这次要处理的轴向
Texture3D<int> WindBufferTarget;
RWTexture3D<int> WindBufferOutput;

uniform int3 VolumeSizeMinusOne;
uniform float AdvectionForce;

#define N 4
// 平流模拟正向平流
// 将一个像素上的风力传递到目标像素上
[numthreads(N,N,N)]
void CSMain (int3 dispatchThreadID : SV_DispatchThreadID)
{
    // 抽取目标轴向纹理数据
    float targetData = PackIntToFloat(WindBufferTarget[dispatchThreadID.xyz].r);
    // 抽取三个方向的风力数据
    int3 windDataInt = int3(WindBufferInputX[dispatchThreadID.xyz].r, WindBufferInputY[dispatchThreadID.xyz].r,
                            WindBufferInputZ[dispatchThreadID.xyz].r);
    float3 advectionData = windDataInt * AdvectionForce * FXDPT_SIZE_R;
    int3 moveCell = (int3)(floor(advectionData + dispatchThreadID));
    // 指定当前格子和周围格子的偏移比例
    float3 offsetNeb = frac(advectionData);
    float3 offsetOri = 1.0 - offsetNeb;

    // 根据风向偏移到指定的格子后，开始计算各个方向的平流
    if(all(moveCell >= 0 && moveCell <= VolumeSizeMinusOne))
    {
        float adData = offsetOri.x * offsetOri.y * offsetOri.z * targetData;
        InterlockedAdd(WindBufferOutput[moveCell.xyz], PackFloatToInt(adData));
    }
    // 目标中心x+1
    int3 tempCell = moveCell + int3(1, 0, 0);
    if(all(tempCell >= 0 && tempCell <= VolumeSizeMinusOne))
    {
        float adData = offsetNeb.x * offsetOri.y * offsetOri.z * targetData;
        InterlockedAdd(WindBufferOutput[tempCell.xyz], PackFloatToInt(adData));
    }
    // 目标中心z+1(但战神中的yz是反转的,因此整理完之后要将其归位置,这里实际上就是y+1)
    // 不偏移的部分取advectionData分量,否则取advectionDataFrac分量
    tempCell = moveCell + int3(0, 1, 0);
    if(all(tempCell >= 0 && tempCell <= VolumeSizeMinusOne))
    {
        float adData = offsetOri.x * offsetNeb.y * offsetOri.z * targetData;
        InterlockedAdd(WindBufferOutput[tempCell.xyz], PackFloatToInt(adData));
    }
    
    tempCell = moveCell + int3(1, 1, 0);
    if(all(tempCell >= 0 && tempCell <= VolumeSizeMinusOne))
    {
        float adData = offsetNeb.x * offsetNeb.y * offsetOri.z * targetData;
        InterlockedAdd(WindBufferOutput[tempCell.xyz], PackFloatToInt(adData));
    }

    tempCell = moveCell + int3(0, 0, 1);
    if(all(tempCell >= 0 && tempCell <= VolumeSizeMinusOne))
    {
        float adData = offsetOri.x * offsetOri.y * offsetNeb.z * targetData;
        InterlockedAdd(WindBufferOutput[tempCell.xyz], PackFloatToInt(adData));
    }

    tempCell = moveCell + int3(1, 0, 1);
    if(all(tempCell >= 0 && tempCell <= VolumeSizeMinusOne))
    {
        float adData = offsetNeb.x * offsetOri.y * offsetNeb.z * targetData;
        InterlockedAdd(WindBufferOutput[tempCell.xyz], PackFloatToInt(adData));
    }

    tempCell = moveCell + int3(0, 1, 1);
    if(all(tempCell >= 0 && tempCell <= VolumeSizeMinusOne))
    {
        float adData = offsetOri.x * offsetNeb.y * offsetNeb.z * targetData;
        InterlockedAdd(WindBufferOutput[tempCell.xyz], PackFloatToInt(adData));
    }

    tempCell = moveCell + int3(1, 1, 1);
    if(all(tempCell >= 0 && tempCell <= VolumeSizeMinusOne))
    {
        float adData = offsetNeb.x * offsetNeb.y * offsetNeb.z * targetData;
        InterlockedAdd(WindBufferOutput[tempCell.xyz], PackFloatToInt(adData));
    }
}

// 平流模拟反向平流
// 这个模拟的是从反平流方向的一个3x3区域的风力信息传递到目标像素上
[numthreads(N,N,N)]
void CSMain2 (int3 dispatchThreadID : SV_DispatchThreadID)
{
    // 抽取三个方向的风力数据
    float3 windDataInt = PackIntToFloat(int3(WindBufferInputX[dispatchThreadID.xyz].r, WindBufferInputY[dispatchThreadID.xyz].r,
                            WindBufferInputZ[dispatchThreadID.xyz].r));
    float3 advectionData = windDataInt * -AdvectionForce;
    int3 moveCell = (int3)(floor(advectionData + dispatchThreadID));
    // 指定当前格子和周围格子的偏移比例
    float3 offsetNeb = frac(advectionData);
    float3 offsetOri = 1.0 - offsetNeb;
    // 抽取目标轴向纹理数据
    float targetData = PackIntToFloat(WindBufferTarget[moveCell.xyz].r);
    targetData *= offsetOri.x * offsetOri.y * offsetOri.z;

    int3 tempPos1 = moveCell.xyz + int3(1, 0, 0);
    float targetDataX1 = PackIntToFloat(WindBufferTarget[tempPos1.xyz].r);
    targetDataX1 *= offsetNeb.x * offsetOri.y * offsetOri.z;

    int3 tempPos2 = moveCell.xyz + int3(0, 1, 0);
    float targetDataY1 = PackIntToFloat(WindBufferTarget[tempPos2.xyz].r);
    targetDataY1 *= offsetOri.x * offsetNeb.y * offsetOri.z;

    int3 tempPos3 = moveCell.xyz + int3(1, 1, 0);
    float targetDataX1Y1 = PackIntToFloat(WindBufferTarget[tempPos3.xyz].r);
    targetDataX1Y1 *= offsetNeb.x * offsetNeb.y * offsetOri.z;

    int3 tempPos4 = moveCell.xyz + int3(0, 0, 1);
    float targetDataZ1 = PackIntToFloat(WindBufferTarget[tempPos4.xyz].r);
    targetDataZ1 *= offsetOri.x * offsetOri.y * offsetNeb.z;

    int3 tempPos5 = moveCell.xyz + int3(1, 0, 1);
    float targetDataX1Z1 = PackIntToFloat(WindBufferTarget[tempPos5.xyz].r);
    targetDataX1Z1 *= offsetNeb.x * offsetOri.y * offsetNeb.z;

    int3 tempPos6 = moveCell.xyz + int3(0, 1, 1);
    float targetDataY1Z1 = PackIntToFloat(WindBufferTarget[tempPos6.xyz].r);
    targetDataY1Z1 *= offsetOri.x * offsetNeb.y * offsetNeb.z;

    int3 tempPos7 = moveCell.xyz + int3(1, 1, 1);
    float targetDataX1Y1Z1 = PackIntToFloat(WindBufferTarget[tempPos7.xyz].r);
    targetDataX1Y1Z1 *= offsetNeb.x * offsetNeb.y * offsetNeb.z;

    if(all(moveCell >= 0 && moveCell <= VolumeSizeMinusOne))
    {
        InterlockedAdd(WindBufferOutput[moveCell.xyz], -PackFloatToInt(targetData));
    }
    if(all(tempPos1 >= 0 && tempPos1 <= VolumeSizeMinusOne))
    {
        InterlockedAdd(WindBufferOutput[tempPos1.xyz], -PackFloatToInt(targetDataX1));
    }
    if(all(tempPos2 >= 0 && tempPos2 <= VolumeSizeMinusOne))
    {
        InterlockedAdd(WindBufferOutput[tempPos2.xyz], -PackFloatToInt(targetDataY1));
    }
    if(all(tempPos3 >= 0 && tempPos3 <= VolumeSizeMinusOne))
    {
        InterlockedAdd(WindBufferOutput[tempPos3.xyz], -PackFloatToInt(targetDataX1Y1));
    }
    if(all(tempPos4 >= 0 && tempPos4 <= VolumeSizeMinusOne))
    {
        InterlockedAdd(WindBufferOutput[tempPos4.xyz], -PackFloatToInt(targetDataZ1));
    }
    if(all(tempPos5 >= 0 && tempPos5 <= VolumeSizeMinusOne))
    {
        InterlockedAdd(WindBufferOutput[tempPos5.xyz], -PackFloatToInt(targetDataX1Z1));
    }
    if(all(tempPos6 >= 0 && tempPos6 <= VolumeSizeMinusOne))
    {
        InterlockedAdd(WindBufferOutput[tempPos6.xyz], -PackFloatToInt(targetDataY1Z1));
    }
    if(all(tempPos7 >= 0 && tempPos7 <= VolumeSizeMinusOne))
    {
        InterlockedAdd(WindBufferOutput[tempPos7.xyz], -PackFloatToInt(targetDataX1Y1Z1));
    }
    if(all(dispatchThreadID <= VolumeSizeMinusOne))
    {
        float cellData = targetData + targetDataX1 + targetDataY1 + targetDataX1Y1
                        + targetDataZ1 + targetDataX1Z1 + targetDataY1Z1
                        + targetDataX1Y1Z1;
        InterlockedAdd(WindBufferOutput[dispatchThreadID.xyz], PackFloatToInt(cellData));
    }
}
The final wind speed information from each channel is stored in a single 3D RT image.
The logic for this part is relatively simple; you just need to pass the corresponding RT into the ComputeShader for channel blending.

#pragma kernel CSMain

Texture3D<int> WindBufferInputX;
Texture3D<int> WindBufferInputY;
Texture3D<int> WindBufferInputZ;
RWTexture3D<float3> WindBufferOutput;

#define N 4

[numthreads(N,N,N)]
void CSMain (int3 dispatchThreadID : SV_DispatchThreadID)
{
    float x = PackIntToFloat(WindBufferInputX[dispatchThreadID.xyz].r);
    float y = PackIntToFloat(WindBufferInputY[dispatchThreadID.xyz].r);
    float z = PackIntToFloat(WindBufferInputZ[dispatchThreadID.xyz].r);
    WindBufferOutput[dispatchThreadID.xyz] = float3(x,y,z);
}
Performance on mobile devices
The phone I'm using is my own Huawei Mate 30 Pro.

First, when releasing, you need to select Vulcan (this is very important).

Currently, it seems that only the Vulcan platform supports ComputeShader.

The overall rendering time is shown in the image below, which seems acceptable.

The frame rate is low because of GPU Instancing issues. I set it to a very high level to see the effect, with several million polygons...

The grass was created using a geometry shader; you can refer to this repositories for details.

The changes are minor; the wind force values ​​have been replaced with 3DRT values, and the algorithm for the vertex animation has been adjusted.

https://github.com/wlgys8/URPLearn/tree/master/Assets/URPLearn/GrassGPUInstances
github.com/wlgys8/URPLearn/tree/master/Assets/URPLearn/GrassGPUInstances

Figure 11 Performance graph (trough)

Figure 12 Performance graph (peak)
Finally, add a little something extra.
The UI was hastily put together and debugged; it's very ugly and lacks features, but the main purpose was to get it running on a mobile phone.

It allows you to control camera movement, create and move motors, adjust certain motor properties, and view the final RT output of wind information.


I'll upload the final version of the project to Git after I've finished organizing the code. Stay tuned!

(This is my first post on Zhihu, so the content doesn't have much technical content. If there's anything I've said that's not good or correct, I hope the experts can point it out. (heart emoji))

Project repository: GitHub - SaberZG/GodOfWarWindSimulation: The wind field system of God of War 4, replicated using both Unity Compute Shader and ECS.