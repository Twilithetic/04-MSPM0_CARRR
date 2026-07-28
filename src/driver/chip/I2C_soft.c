//******************************** */
//            I2C 实现
//******************************** */
#include <stdint.h>
#include <stdbool.h>

// ---- 弱函数跨平台兼容（兼容 Keil __weak / GCC+tiarmclang __attribute__((weak))） ----
#ifndef __weak
#define __weak __attribute__((weak))
#endif

// ---- 弱函数默认实现（跨平台时由用户在自己的平台代码中覆盖） ----

__weak void I2C_SDA_Set(void)   {}
__weak void I2C_SDA_Clr(void)   {}
__weak void I2C_SCL_Set(void)   {}
__weak void I2C_SCL_Clr(void)   {}
__weak uint8_t I2C_SDA_Read(void) { return 0; }
__weak void I2C_delay_us(uint8_t us) { (void)us; }

// ----------------------------------------------------------------

/// @brief 在SCL 置1时 如果SDA从1到0 是开始时序（读取数据时 SCL同样SCL置1 但SDA不应该变化）
/// @param
void I2C_Start(void){
    I2C_SDA_Set();
    I2C_delay_us(1);
    I2C_SCL_Set();
    I2C_delay_us(1);
    I2C_SDA_Clr();
    I2C_delay_us(1); 
}


/// @brief 发送一个bit SCL置0时 放主机要发送的数据，SCL置1时 从机读取数据 从SDA上
/// @param byte 
void I2C_SendByte(uint8_t byte){

    for (uint8_t i = 0; i < 8; i++)
    {
        
        I2C_SCL_Clr();
        I2C_delay_us(1);
        if (byte & 0x80)
        {
            I2C_SDA_Set();
        }
        else
        {
            I2C_SDA_Clr();
        }
        I2C_delay_us(1);
        I2C_SCL_Set(); // 此时从机 会读取SDA线上的电平来获取数据， 他们想两座离的很远的房子一样。
        I2C_delay_us(1);
        byte <<= 1;
    }
    //I2C_SCL_Clr();
    
}


/// @brief 主机接受从机确认收到字节的应答 主机的每个字节发送完成后，主机会释放SDA（SDA置1），如果从机按约定在此时拉低（SDA置0），并且主机发现有人拉低了，那么主机发送的字节就确认被收到了
/// @param  
/// @return  如果确认收到了(此时AckBit == 0)，会返回True（stdbool.h提供），由于!AckBit
bool I2C_ReceiveAck(void){
    I2C_SCL_Clr();
    I2C_delay_us(1);
    I2C_SDA_Set();// 释放SDA
    I2C_delay_us(1);
    I2C_SCL_Set();// 担心！ 万一在SCL置1时下个指令是置SDA为0 这不成Start时序了吗(。_。) 。其实不担心，下个无非是接受或发送字节的程序，他们都会在发送前将 SCL置0的，但 I2C_SendAck_Continue()就没那么好运了
    I2C_delay_us(2); // 只是在示波器上好看
    uint8_t AckBit = I2C_SDA_Read();
    
    return !AckBit;

}


/// @brief 接收一个Bit SCL置0时 从机放要给的数据，SCL置1时 主机读取数据在SDA上
/// @param  
/// @return 
uint8_t I2C_ReceiveByte(void){
    uint8_t byte = 0x00;
    // 
    I2C_SDA_Set(); // 乌鸡之谈 -->主机是开漏输出，如果此时从机的最后一个bit数据是0，不置1松开SDA的话（开漏输出不是推挽输出，只能拉低或者撒手不管（置1）），从机的数据就全是0了
    I2C_delay_us(1);
    for (uint8_t i = 0; i < 8; i++)
    {
        I2C_SCL_Clr();
        I2C_delay_us(1);   
        I2C_SCL_Set();
        I2C_delay_us(1);
        if(I2C_SDA_Read() == 1){byte |= (0x80 >> i);}
    }

    return byte;

}


/// @brief 从机接受主机确认收到字节的应答，从机发送完一个字节的数据，从机会释放SDA（SDA置1），如果主机按约定在此时拉低SDA（SDA置0），并且从机发现了，拿那从机发送的字节就确认被收到啦(●'◡'●)。
void I2C_SendAck_Continue(){
    I2C_SCL_Clr();
    I2C_delay_us(1);
    I2C_SDA_Clr();
    I2C_delay_us(1);
    I2C_SCL_Set();
    I2C_delay_us(2); // 只是在示波器上好看
    I2C_SCL_Clr();
    I2C_SDA_Set(); // 先 Clr置0 再SDA松手， 不影响后面的时序

}


/// @brief 从机接受主机确认收到字节的应答，从机发送完一个字节的数据，从机会释放SDA（SDA置1），如果主机按约定在此时拉低SDA（SDA置0），并且从机发现了，拿那从机发送的字节就确认被收到啦(●'◡'●)。
void I2C_SendAck_Done(){
    I2C_SCL_Clr();
    I2C_delay_us(1);
    I2C_SDA_Set();
    I2C_delay_us(1);
    I2C_SCL_Set();
    I2C_delay_us(1);
}


/// @brief 在SCL 置1时 如果SDA从0到1 是停止时序（读取数据时 SCL同样SCL置1 但SDA不应该变化）
/// @param  
void I2C_Stop(void){
    I2C_SCL_Clr();
    I2C_delay_us(1);
    I2C_SDA_Clr();
    I2C_delay_us(1);
    I2C_SCL_Set();
    I2C_delay_us(1);
    I2C_SDA_Set();
    I2C_delay_us(1);
}