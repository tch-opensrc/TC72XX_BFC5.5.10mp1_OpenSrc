//------------------------------------------------------------------------
// sed1356.h: SED1356 LCD/CRT Controllers - 16-Bit access mode
//
// Copyright (C) 2003 Gary Thomas

//------------------------------------------------
// Bit position defines
#define BIT0     0x00000001
#define BIT1     0x00000002
#define BIT2     0x00000004
#define BIT3     0x00000008
#define BIT4     0x00000010
#define BIT5     0x00000020
#define BIT6     0x00000040
#define BIT7     0x00000080
#define BIT8     0x00000100
#define BIT9     0x00000200
#define BIT10    0x00000400
#define BIT11    0x00000800
#define BIT12    0x00001000
#define BIT13    0x00002000
#define BIT14    0x00004000
#define BIT15    0x00008000
#define BIT16    0x00010000
#define BIT17    0x00020000
#define BIT18    0x00040000
#define BIT19    0x00080000
#define BIT20    0x00100000
#define BIT21    0x00200000
#define BIT22    0x00400000
#define BIT23    0x00800000
#define BIT24    0x01000000
#define BIT25    0x02000000
#define BIT26    0x04000000
#define BIT27    0x08000000
#define BIT28    0x10000000
#define BIT29    0x20000000
#define BIT30    0x40000000
#define BIT31    0x80000000


//------------------------------------------------------------------------
// cpu specific code must define the following board specific macros.
// in cpuio.h.  These examples assume the SED135x has been placed in 
// the correct endian mode via hardware.
// #define SED_MEM_BASE   0xf0600000 <-- just example addresses, 
// #define SED_REG_BASE    0xf0400000 <-- define for each board
// #define SED_STEP      1 <-- 1 = device is on 16-bit boundry, 2 = 32-bit boundry, 4 = 64-bit boundry
// #define SED_REG16(_x_)    *(vushortr *)(SED_REG_BASE + (_x_ * SED_STEP))  // Control/Status Registers
// #define RD_FB16(_reg_,_val_) ((_val_) = *((vushort *)((SED_MEM_BASE + (_reg_ * SED_STEP)))))
// #define WR_FB16(_reg_,_val_) (*((vushort *)((SED_MEM_BASE + (_reg_ * 2)))) = (_val_))
// Big endian processors
// #define H2SED(_x_)  ((((x) & 0xff00U) >> 8) | (((x) & 0x00ffU) << 8))
// Little endian
// #define H2SED(_x_)  (_x_)
    

// ----------------------------------------------------------
// SED1356 registers - 16-Bit Access Mode.  The first register
// referenced is the even addressed register.  The byte offsets
// of the odd registers are shown in the comments
#define SED1356_REG_REV_and_MISC         SED_REG16(0x00) 
//  #define SED1356_REG_MISC                    SED_REG(0x01)
#define SED1356_REG_GPIO_CFG             SED_REG16(0x04)
#define SED1356_REG_GPIO_CTL             SED_REG16(0x08)
#define SED1356_REG_MD_CFG_RD_LO_and_HI  SED_REG16(0x0c) 
//  #define SED1356_REG_MD_CFG_RD_HI          SED_REG(0x0d)
#define SED1356_REG_MCLK_CFG             SED_REG16(0x10)
#define SED1356_REG_LCD_PCLK_CFG         SED_REG16(0x14)
#define SED1356_REG_CRT_PCLK_CFG         SED_REG16(0x18)
#define SED1356_REG_MEDIA_PCLK_CFG       SED_REG16(0x1c)
#define SED1356_REG_WAIT_STATE           SED_REG16(0x1e)
#define SED1356_REG_MEM_CFG_and_REF_RATE SED_REG16(0x20) 
//  #define SED1356_REG_REF_RATE                SED_REG(0x21)
#define SED1356_REG_MEM_TMG0_and_1       SED_REG16(0x2a) 
//  #define SED1356_REG_MEM_TMG1               SED_REG(0x2b)
#define SED1356_REG_PANEL_TYPE_and_MOD_RATE SED_REG16(0x30) 
//  #define SED1356_REG_MOD_RATE                SED_REG(0x31)
// LCD Control registers
#define SED1356_REG_LCD_HOR_DISP              SED_REG16(0x32)
#define SED1356_REG_LCD_HOR_NONDISP_and_START SED_REG16(0x34) 
//  #define SED1356_REG_LCD_HOR_START         SED_REG(0x35)
#define SED1356_REG_LCD_HOR_PULSE             SED_REG16(0x36)
#define SED1356_REG_LCD_VER_DISP_HT_LO_and_HI SED_REG16(0x38) 
//  #define SED1356_REG_LCD_VER_DISP_HT_HI      SED_REG(0x39)
#define SED1356_REG_LCD_VER_NONDISP_and_START SED_REG16(0x3a) 
//  #define SED1356_REG_LCD_VER_START          SED_REG(0x3b)
#define SED1356_REG_LCD_VER_PULSE             SED_REG16(0x3c)
#define SED1356_REG_LCD_DISP_MODE_and_MISC    SED_REG16(0x40) 
//  #define SED1356_REG_LCD_MISC        SED_REG(0x41)
#define SED1356_REG_LCD_DISP_START_LO_and_MID SED_REG16(0x42) 
//  #define SED1356_REG_LCD_DISP_START_MID     SED_REG(0x43)
#define SED1356_REG_LCD_DISP_START_HI         SED_REG16(0x44)
#define SED1356_REG_LCD_ADD_OFFSET_LO_and_HI  SED_REG16(0x46) 
//  #define SED1356_REG_LCD_ADD_OFFSET_HI       SED_REG(0x47)
#define SED1356_REG_LCD_PIXEL_PAN             SED_REG16(0x48)  
#define SED1356_REG_LCD_FIFO_THRESH_LO_and_HI SED_REG16(0x4a) 
//  #define SED1356_REG_LCD_FIFO_LO_THRES      SED_REG(0x4b)
// CRT/TV Control registers
#define SED1356_REG_CRT_HOR_DISP              SED_REG16(0x50)
#define SED1356_REG_CRT_HOR_NONDISP_and_START SED_REG16(0x52) 
//  #define SED1356_REG_CRT_HOR_START         SED_REG(0x53)
#define SED1356_REG_CRT_HOR_PULSE             SED_REG16(0x54)
#define SED1356_REG_CRT_VER_DISP_HT_LO_and_HI SED_REG16(0x56) 
//  #define SED1356_REG_CRT_VER_DISP_HT_HI      SED_REG(0x57)
#define SED1356_REG_CRT_VER_NONDISP_and_START SED_REG16(0x58) 

//  #define SED1356_REG_CRT_VER_START          SED_REG(0x59)
#define SED1356_REG_CRT_VER_PULSE_and_OUT_CTL    SED_REG16(0x5a) 
//  #define SED1356_REG_CRT_OUT_CTL        SED_REG(0x5b)
#define SED1356_REG_CRT_DISP_MODE          SED_REG16(0x60)
#define SED1356_REG_CRT_DISP_START_LO_and_MID       SED_REG16(0x62) 
//  #define SED1356_REG_CRT_DISP_START_MID     SED_REG(0x63)
#define SED1356_REG_CRT_DISP_START_HI           SED_REG16(0x64)
#define SED1356_REG_CRT_ADD_OFFSET_LO_and_HI       SED_REG16(0x66) 
//  #define SED1356_REG_CRT_ADD_OFFSET_HI       SED_REG(0x67)
#define SED1356_REG_CRT_PIXEL_PAN             SED_REG16(0x68)
#define SED1356_REG_CRT_FIFO_THRESH_LO_and_HI      SED_REG16(0x6a) 
//  #define SED1356_REG_CRT_FIFO_LO_THRES      SED_REG(0x6b)

// LCD Cursor Control Registers
#define SED1356_REG_LCD_CURSOR_CTL_and_START_ADD  SED_REG16(0x70) 
//  #define SED1356_REG_LCD_CURSOR_START_ADD  SED_REG(0x71)
#define SED1356_REG_LCD_CURSOR_X_POS_LO_and_HI     SED_REG16(0x72) 
//  #define SED1356_REG_LCD_CURSOR_X_POS_HI   SED_REG(0x73)
#define SED1356_REG_LCD_CURSOR_Y_POS_LO_and_HI    SED_REG16(0x74) 
//  #define SED1356_REG_LCD_CURSOR_Y_POS_HI    SED_REG(0x75)
#define SED1356_REG_LCD_CURSOR_BLUE_and_GREEN_CLR_0  SED_REG16(0x76) 
//  #define SED1356_REG_LCD_CURSOR_GREEN_CLR_0  SED_REG(0x77)
#define SED1356_REG_LCD_CURSOR_RED_CLR_0      SED_REG16(0x78)  
#define SED1356_REG_LCD_CURSOR_BLUE_and_GREEN_CLR_1  SED_REG16(0x7a) 
//  #define SED1356_REG_LCD_CURSOR_GREEN_CLR_1  SED_REG(0x7b)
#define SED1356_REG_LCD_CURSOR_RED_CLR_1      SED_REG16(0x7c)
#define SED1356_REG_LCD_CURSOR_FIFO_THRESH      SED_REG16(0x7e)

// CRT Cursor Control Registers
#define SED1356_REG_CRT_CURSOR_CTL_and_START_ADD  SED_REG16(0x80) 
//  #define SED1356_REG_CRT_CURSOR_START_ADD  SED_REG(0x81)
#define SED1356_REG_CRT_CURSOR_X_POS_LO_and_HI     SED_REG16(0x82) 
//  #define SED1356_REG_CRT_CURSOR_X_POS_HI   SED_REG(0x83)
#define SED1356_REG_CRT_CURSOR_Y_POS_LO_and_HI    SED_REG16(0x84) 
//  #define SED1356_REG_CRT_CURSOR_Y_POS_HI    SED_REG(0x85)
#define SED1356_REG_CRT_CURSOR_BLUE_and_GREEN_CLR_0  SED_REG16(0x86) 
//  #define SED1356_REG_CRT_CURSOR_GREEN_CLR_0  SED_REG(0x87)
#define SED1356_REG_CRT_CURSOR_RED_CLR_0      SED_REG16(0x88)
#define SED1356_REG_CRT_CURSOR_BLUE_and_GREEN_CLR_1  SED_REG16(0x8a) 
//  #define SED1356_REG_CRT_CURSOR_GREEN_CLR_1  SED_REG(0x8b)
#define SED1356_REG_CRT_CURSOR_RED_CLR_1      SED_REG16(0x8c)
#define SED1356_REG_CRT_CURSOR_FIFO_THRESH      SED_REG16(0x8e)

// BitBlt Control Registers
#define SED1356_REG_BLT_CTL_0_and_1          SED_REG16(0x100) 
//  #define SED1356_REG_BLT_CTL_1        SED_REG(0x101)
#define SED1356_REG_BLT_ROP_CODE_and_BLT_OP      SED_REG16(0x102) 
//  #define SED1356_REG_BLT_OP          SED_REG(0x103)
#define SED1356_REG_BLT_SRC_START_LO_and_MID    SED_REG16(0x104) 
//  #define SED1356_REG_BLT_SRC_START_MID    SED_REG(0x105)
#define SED1356_REG_BLT_SRC_START_HI        SED_REG16(0x106)
#define SED1356_REG_BLT_DEST_START_LO_and_MID    SED_REG16(0x108) 
//  #define SED1356_REG_BLT_DEST_START_MID    SED_REG(0x109)
#define SED1356_REG_BLT_DEST_START_HI        SED_REG16(0x10a)
#define SED1356_REG_BLT_ADD_OFFSET_LO_and_HI    SED_REG16(0x10c) 
//  #define SED1356_REG_BLT_ADD_OFFSET_HI    SED_REG(0x10d)
#define SED1356_REG_BLT_WID_LO_and_HI        SED_REG16(0x110) 
//  #define SED1356_REG_BLT_WID_HI        SED_REG(0x111)
#define SED1356_REG_BLT_HGT_LO_and_HI        SED_REG16(0x112) 
//  #define SED1356_REG_BLT_HGT_HI        SED_REG(0x113)
#define SED1356_REG_BLT_BG_CLR_LO_and_HI      SED_REG16(0x114) 
//  #define SED1356_REG_BLT_BG_CLR_HI      SED_REG(0x115)
#define SED1356_REG_BLT_FG_CLR_LO_and_HI      SED_REG16(0x118) 
//  #define SED1356_REG_BLT_FG_CLR_HI      SED_REG(0x119)

// Look-Up Table Control Registers
#define SED1356_REG_LUT_MODE            SED_REG16(0x1e0)
#define SED1356_REG_LUT_ADD              SED_REG16(0x1e2)
#define SED1356_REG_LUT_DATA            SED_REG16(0x1e4)

// Power and Miscellaneous Control Registers
#define SED1356_REG_PWR_CFG_and_STAT        SED_REG16(0x1f0) 
//  #define SED1356_REG_PWR_STAT        SED_REG(0x1f1)
#define SED1356_REG_WATCHDOG_CTL          SED_REG16(0x1f4)
#define SED1356_REG_DISP_MODE            SED_REG16(0x1fc)


//------------------------------------------------------------------------------------------------------------

// Bit Assignments - Little Endian, Use H2SED() macro to access

//

// SED1356_REG_REV_and_MISC - even
#define SED1356_REV_ID_MASK          0xfc    
// ID bits - masks off the rev bits
#define SED1356_REV_ID_1356          BIT4
#define SED1356_REV_ID_1355          BIT3


// SED1356_REG_REV_and_MISC - odd
#define SED1356_MISC_HOST_DIS        BIT7 << 8  
// 0 = enable host access, 1 = disable


// SED1356_REG_GPIO_CFG and SED1356_REG_GPIO_STAT
#define SED1356_GPIO_GPIO3           BIT3    
// 0 = input, 1 = output, if configured as GPIO
#define SED1356_GPIO_GPIO2           BIT2
#define SED1356_GPIO_GPIO1           BIT1


// SED1356_REG_MCLK_CFG
#define SED1356_MCLK_DIV2            BIT4 
#define SED1356_MCLK_SRC_BCLK        BIT0 
#define SED1356_MCLK_SRC_CLKI        0x00


// SED1356_REG_LCD_PCLK_CFG, SED1356_REG_CRT_PCLK_CFG 

// and SED1356_REG_MEDIA_PCLK_CFG
#define SED1356_PCLK_X2            BIT7    
// SED1356_REG_CRT_PCLK_CFG only
#define SED1356_PCLK_DIV1          0x00 << 4
#define SED1356_PCLK_DIV2          0x01 << 4
#define SED1356_PCLK_DIV3          0x02 << 4
#define SED1356_PCLK_DIV4          0x03 << 4
#define SED1356_PCLK_SRC_CLKI      0x00
#define SED1356_PCLK_SRC_BCLK      0x01
#define SED1356_PCLK_SRC_CLKI2     0x02
#define SED1356_PCLK_SRC_MCLK      0x03


// SED1356_REG_MEM_CFG_and_REF_RATE - even
#define SED1356_MEM_CFG_2CAS_EDO   0x00   
#define SED1356_MEM_CFG_2CAS_FPM   0x01   
#define SED1356_MEM_CFG_2WE_EDO    0x02   
#define SED1356_MEM_CFG_2WE_FPM    0x03   
#define SED1356_MEM_CFG_MASK       0x03   


// SED1356_REG_MEM_CFG_and_REF_RATE - odd
#define SED1356_REF_TYPE_CBR       0x00 << 6 << 8
#define SED1356_REF_TYPE_SELF      0x01 << 6 << 8
#define SED1356_REF_TYPE_NONE      0x02 << 6 << 8
#define SED1356_REF_TYPE_MASK      0x03 << 6 << 8
#define SED1356_REF_RATE_64        0x00 << 0 << 8  
// MCLK / 64
#define SED1356_REF_RATE_128       0x01 << 0 << 8  
// MCLK / 128
#define SED1356_REF_RATE_256       0x02 << 0 << 8  
// MCLK / 256
#define SED1356_REF_RATE_512       0x03 << 0 << 8  
// MCLK / 512
#define SED1356_REF_RATE_1024      0x04 << 0 << 8  
// MCLK / 1024
#define SED1356_REF_RATE_2048      0x05 << 0 << 8  
// MCLK / 2048
#define SED1356_REF_RATE_4096      0x06 << 0 << 8  
// MCLK / 4096
#define SED1356_REF_RATE_8192      0x07 << 0 << 8  
// MCLK / 8192
#define SED1356_REF_RATE_MASK      0x07 << 0 << 8  
// MCLK / 8192


// SED1356_REG_MEM_TMG0_and_1 - even
#define SED1356_MEM_TMG0_EDO50_MCLK40    0x01
#define SED1356_MEM_TMG0_EDO50_MCLK33    0x01
#define SED1356_MEM_TMG0_EDO60_MCLK33    0x01
#define SED1356_MEM_TMG0_EDO50_MCLK30    0x12
#define SED1356_MEM_TMG0_EDO60_MCLK30    0x01
#define SED1356_MEM_TMG0_EDO70_MCLK30    0x00
#define SED1356_MEM_TMG0_EDO50_MCLK25    0x12
#define SED1356_MEM_TMG0_EDO60_MCLK25    0x12
#define SED1356_MEM_TMG0_EDO70_MCLK25    0x01
#define SED1356_MEM_TMG0_EDO80_MCLK25    0x00
#define SED1356_MEM_TMG0_EDO50_MCLK20    0x12
#define SED1356_MEM_TMG0_EDO60_MCLK20    0x12
#define SED1356_MEM_TMG0_EDO70_MCLK20    0x12
#define SED1356_MEM_TMG0_EDO80_MCLK20    0x01
#define SED1356_MEM_TMG0_FPM50_MCLK25    0x12
#define SED1356_MEM_TMG0_FPM60_MCLK25    0x01
#define SED1356_MEM_TMG0_FPM50_MCLK20    0x12
#define SED1356_MEM_TMG0_FPM60_MCLK20    0x12
#define SED1356_MEM_TMG0_FPM70_MCLK20    0x11
#define SED1356_MEM_TMG0_FPM80_MCLK20    0x01


// SED1356_REG_MEM_TMG0_and_1 - odd
#define SED1356_MEM_TMG1_EDO50_MCLK40    0x01 << 8
#define SED1356_MEM_TMG1_EDO50_MCLK33    0x01 << 8
#define SED1356_MEM_TMG1_EDO60_MCLK33    0x01 << 8
#define SED1356_MEM_TMG1_EDO50_MCLK30    0x02 << 8
#define SED1356_MEM_TMG1_EDO60_MCLK30    0x01 << 8
#define SED1356_MEM_TMG1_EDO70_MCLK30    0x00 << 8
#define SED1356_MEM_TMG1_EDO50_MCLK25    0x02 << 8
#define SED1356_MEM_TMG1_EDO60_MCLK25    0x02 << 8
#define SED1356_MEM_TMG1_EDO70_MCLK25    0x01 << 8
#define SED1356_MEM_TMG1_EDO80_MCLK25    0x01 << 8
#define SED1356_MEM_TMG1_EDO50_MCLK20    0x02 << 8
#define SED1356_MEM_TMG1_EDO60_MCLK20    0x02 << 8
#define SED1356_MEM_TMG1_EDO70_MCLK20    0x02 << 8
#define SED1356_MEM_TMG1_EDO80_MCLK20    0x01 << 8
#define SED1356_MEM_TMG1_FPM50_MCLK25    0x02 << 8
#define SED1356_MEM_TMG1_FPM60_MCLK25    0x01 << 8
#define SED1356_MEM_TMG1_FPM50_MCLK20    0x02 << 8
#define SED1356_MEM_TMG1_FPM60_MCLK20    0x02 << 8
#define SED1356_MEM_TMG1_FPM70_MCLK20    0x02 << 8
#define SED1356_MEM_TMG1_FPM80_MCLK20    0x01 << 8



// Bit definitions                         

//

// SED1356_REG_PANEL_TYPE_AND_MOD_RATE - even
#define SED1356_PANEL_TYPE_EL      BIT7
#define SED1356_PANEL_TYPE_4_9     (0x00 << 4)    
// Passive 4-Bit, TFT 9-Bit
#define SED1356_PANEL_TYPE_8_12    (0x01 << 4)    
// Passive 8-Bit, TFT 12-Bit
#define SED1356_PANEL_TYPE_16      (0x02 << 4)    
// Passive 16-Bit, or TFT 18-Bit
#define SED1356_PANEL_TYPE_MASK    (0x03 << 4)
#define SED1356_PANEL_TYPE_FMT     BIT3      
// 0 = Passive Format 1, 1 = Passive Format 2
#define SED1356_PANEL_TYPE_CLR     BIT2      
// 0 = Passive Mono, 1 = Passive Color
#define SED1356_PANEL_TYPE_DUAL    BIT1      
// 0 = Passive Single, 1 = Passive Dual
#define SED1356_PANEL_TYPE_TFT     BIT0      
// 0 = Passive, 1 = TFT (DUAL, FMT & CLR are don't cares)


// SED1356_REG_CRT_HOR_PULSE, SED1356_REG_CRT_VER_PULSE,

// SED1356_REG_LCD_HOR_PULSE and SED1356_REG_LCD_VER_PULSE
#define SED1356_PULSE_POL_HIGH     BIT7      
// 0 = CRT/TFT Pulse is Low, Passive is High, 1 = CRT/TFT Pulse is High, Passive is Low
#define SED1356_PULSE_POL_LOW      0x00      
// 0 = CRT/TFT Pulse is Low, Passive is High, 1 = CRT/TFT Pulse is High, Passive is Low
#define SED1356_PULSE_WID(_x_)     (_x_ & 0x0f)  
// Pulse Width in Pixels


// SED1356_LCD_DISP_MODE_and_MISC - even        
#define SED1356_LCD_DISP_BLANK     BIT7      
// 1 = Blank LCD Display
#define SED1356_LCD_DISP_SWIV_NORM (0x00 << 4)    
// Used with SED1356_REG_DISP_MODE Bit 6
#define SED1356_LCD_DISP_SWIV_90   (0x00 << 4)
#define SED1356_LCD_DISP_SWIV_180  (0x01 << 4)
#define SED1356_LCD_DISP_SWIV_270  (0x01 << 4)
#define SED1356_LCD_DISP_SWIV_MASK (0x01 << 4)
#define SED1356_LCD_DISP_16BPP     0x05      
// Bit Per Pixel Selection
#define SED1356_LCD_DISP_15BPP     0x04
#define SED1356_LCD_DISP_8BPP      0x03
#define SED1356_LCD_DISP_4BPP      0x02
#define SED1356_LCD_DISP_BPP_MASK  0x07


// SED1356_LCD_DISP_MODE_and_MISC - odd
#define SED1356_LCD_MISC_DITH      BIT1 << 8    
// 1 = Dither Disable, Passive Panel Only
#define SED1356_LCD_MISC_DUAL      BIT0 << 8    
// 1 = Dual Panel Disable, Passive Panel Only


// SED1356_REG_CRT_VER_PULSE_and_OUT_CTL - odd
#define SED1356_CRT_OUT_CHROM      BIT5 << 8    
// 1 = TV Chrominance Filter Enable
#define SED1356_CRT_OUT_LUM        BIT4 << 8    
// 1 = TV Luminance Filter Enable
#define SED1356_CRT_OUT_DAC_LVL    BIT3 << 8    
// 1 = 4.6ma IREF, 0 = 9.2 IREF
#define SED1356_CRT_OUT_SVIDEO     BIT1 << 8    
// 1 = S-Video Output, 0 = Composite Video Output
#define SED1356_CRT_OUT_PAL        BIT0 << 8    
// 1 = PAL Format Output, 0 = NTSC Format Output


// SED1356_REG_CRT_DISP_MODE
#define SED1356_CRT_DISP_BLANK     BIT7      
// 1 = Blank CRT Display
#define SED1356_CRT_DISP_16BPP     0x05      
// Bit Per Pixel Selection
#define SED1356_CRT_DISP_15BPP     0x04
#define SED1356_CRT_DISP_8BPP      0x03
#define SED1356_CRT_DISP_4BPP      0x02
#define SED1356_CRT_DISP_BPP_MASK  0x07


// SED1356_DISP_MODE        
#define SED1356_DISP_SWIV_NORM     (0x00 << 6)    
// Used with SED1356_LCD_DISP_MODE Bit 4
#define SED1356_DISP_SWIV_90       (0x01 << 6)
#define SED1356_DISP_SWIV_180      (0x00 << 6)
#define SED1356_DISP_SWIV_270      (0x01 << 6)
#define SED1356_DISP_MODE_OFF      0x00      
// All Displays Off
#define SED1356_DISP_MODE_LCD      0x01      
// LCD Only
#define SED1356_DISP_MODE_CRT      0x02      
// CRT Only
#define SED1356_DISP_MODE_LCD_CRT  0x03      
// Simultaneous LCD and CRT
#define SED1356_DISP_MODE_TV       0x04      
// TV Only, Flicker Filter Off
#define SED1356_DISP_MODE_TV_LCD   0x05      
// Simultaneous LCD and TV, Flicker Filter Off
#define SED1356_DISP_MODE_TV_FLICK 0x06      
// TV Only, Flicker Filter On
#define SED1356_DISP_MODE_TV_LCD_FLICK  0x07      
// Simultaneous LCD and TV, Flicker Filter On


// SED1356_REG_PWR_CFG and SED1356_REG_PWR_STAT
#define SED1356_PWR_PCLK        BIT1      
// SED1356_REG_PWR_STAT only
#define SED1356_PWR_MCLK        BIT0


// SED1356_REG_VER_NONDISP
#define SED1356_VER_NONDISP        BIT7      
// vertical retrace status 1 = in retrace


// Display size defines
#define PIXELS_PER_ROW     640
#define PIXELS_PER_COL     480
#define BYTES_PER_PIXEL    2
#define COLS_PER_SCREEN    80
#define ROWS_PER_SCREEN    30


// 16-bit pixels are RGB 565 - LSB of RED and BLUE are tied low at the 

// LCD Interface, while the LSB of GREEN is loaded as 0
#define RED_SUBPIXEL(n)    ((n & 0x1f) << 11)
#define GREEN_SUBPIXEL(n)  ((n & 0x1f) << 5)
#define BLUE_SUBPIXEL(n)  ((n & 0x1f) << 0)


// define a simple VGA style 16-color pallette
#define  LU_BLACK    (RED_SUBPIXEL(0x00) | GREEN_SUBPIXEL(0x00) | BLUE_SUBPIXEL(0x00))
#define  LU_BLUE      (RED_SUBPIXEL(0x00) | GREEN_SUBPIXEL(0x00) | BLUE_SUBPIXEL(0x0f))
#define  LU_GREEN    (RED_SUBPIXEL(0x00) | GREEN_SUBPIXEL(0x0f) | BLUE_SUBPIXEL(0x00))
#define  LU_CYAN      (RED_SUBPIXEL(0x00) | GREEN_SUBPIXEL(0x0f) | BLUE_SUBPIXEL(0x0f))
#define  LU_RED      (RED_SUBPIXEL(0x0f) | GREEN_SUBPIXEL(0x00) | BLUE_SUBPIXEL(0x00))
#define  LU_VIOLET    (RED_SUBPIXEL(0x0f) | GREEN_SUBPIXEL(0x00) | BLUE_SUBPIXEL(0x0f))
#define  LU_YELLOW    (RED_SUBPIXEL(0x0f) | GREEN_SUBPIXEL(0x0f) | BLUE_SUBPIXEL(0x00))
#define  LU_GREY      (RED_SUBPIXEL(0x0f) | GREEN_SUBPIXEL(0x0f) | BLUE_SUBPIXEL(0x0f))
#define  LU_WHITE    (RED_SUBPIXEL(0x17) | GREEN_SUBPIXEL(0x17) | BLUE_SUBPIXEL(0x17))
#define  LU_BRT_BLUE    (RED_SUBPIXEL(0x00) | GREEN_SUBPIXEL(0x00) | BLUE_SUBPIXEL(0x1f))
#define  LU_BRT_GREEN  (RED_SUBPIXEL(0x00) | GREEN_SUBPIXEL(0x1f) | BLUE_SUBPIXEL(0x00))
#define  LU_BRT_CYAN    (RED_SUBPIXEL(0x00) | GREEN_SUBPIXEL(0x1f) | BLUE_SUBPIXEL(0x1f))
#define  LU_BRT_RED    (RED_SUBPIXEL(0x1f) | GREEN_SUBPIXEL(0x00) | BLUE_SUBPIXEL(0x00))
#define  LU_BRT_VIOLET  (RED_SUBPIXEL(0x1f) | GREEN_SUBPIXEL(0x00) | BLUE_SUBPIXEL(0x1f))
#define  LU_BRT_YELLOW  (RED_SUBPIXEL(0x1f) | GREEN_SUBPIXEL(0x1f) | BLUE_SUBPIXEL(0x00))
#define  LU_BRT_WHITE  (RED_SUBPIXEL(0x1f) | GREEN_SUBPIXEL(0x1f) | BLUE_SUBPIXEL(0x1f))

static const ushort vga_lookup[] = {
LU_BLACK,        
// 0
LU_BLUE,        
// 1
LU_GREEN,        
// 2
LU_CYAN,        
// 3
LU_RED,          
// 4
LU_GREY,        
// 5
LU_VIOLET,        
// 6
LU_YELLOW,        
// 7
LU_WHITE,        
// 8
LU_BRT_BLUE,      
// 9
LU_BRT_GREEN,      
// 10
LU_BRT_CYAN,      
// 11
LU_BRT_RED,        
// 12
LU_BRT_VIOLET,      
// 13
LU_BRT_YELLOW,      
// 14
LU_BRT_WHITE      
// 15
};


// default foreground and background colors
#define SED_BG_DEF      1
#define SED_FG_DEF      14


// Vertical and Horizontal Pulse, Start and Non-Display values vary depending

// upon the mode.  The following section gives some insight into how the

// values are arrived at.

// ms = milliseconds, us = microseconds, ns = nanoseconds    

// Mhz = Megaherz, Khz = Kiloherz, Hz = Herz   

//

// ***************************************************************************************************   

// CRT Mode is 640x480 @ 72Hz VESA compatible timing.  PCLK = 31.5Mhz (31.75ns)

// ***************************************************************************************************   

//

//                               CRT MODE HORIZONTAL TIMING PARAMETERS   

//

//                                       |<-------Tha------->|   

//                                       |___________________|                     ______   

// Display Enable   _____________________|                   |____________________|      

//                                       |                   |   

// Horizontal Pulse __           ________|___________________|________          __________   

//                    |_________|        |                   |        |________|   

//                    |<- Thp ->|        |                   |        |   

//                    |         |<-Thbp->|                   |        |   

//                    |                                      |<-Thfp->|   

//                    |<----------------------Tht-------------------->|    

//

// Tha  - Active Display Time                      = 640 pixels   

// Thp  - Horizontal Pulse       = 1.27us/31.75ns  =  40 pixels   

// Thbp - Horizontal Front Porch = 1.016us/31.75ns =  32 pixels   

// Thfp - Horizontal Back Porch  = 3.8us/31.75ns   = 120 pixels   

// Tht  - Total Horizontal Time                    = 832 pixels x 32.75ns/pixel = 26.416us or 38.785Khz   

//

// Correlation between horizontal timing parameters and SED registers   
#define SED_HOR_PULSE_WIDTH_CRT 0x07 
// Horizontal Pulse Width Register           = (Thp/8) - 1
#define SED_HOR_PULSE_START_CRT  0x02 
// Horizontal Pulse Start Position Register    = ((Thfp + 2)/8) - 1
#define SED_HOR_NONDISP_CRT    0x17 
// Horizontal Non-Display Period Register     = ((Thp + Thfp + Thbp)/8) - 1
//
//                                CRT MODE VERTICAL TIMING PARAMTERS   
//
//                                       |<-------Tva------->|   
//                                       |___________________|                     ______   
// Display Enable   _____________________|                   |_____________________|      
//                                       |                   |   
// Vertical Pulse   __           ________|___________________|________          __________   
//                    |_________|        |                   |        |________|   
//                    |<- Tvp ->|        |                   |        |   
//                    |         |<-Tvbp->|                   |        |   
//                    |                                      |<-Tvfp->|   
//                    |<----------------------Tvt-------------------->|    
//
// Tva  - Active Display Time   = 480 lines   
// Tvp  - Vertical Pulse        =  3 lines   
// Tvfp - Vertical Front Porch  =   9 lines   
// Tvbp - Vertical Back Porch   =  28 lines   
// Tvt  - Total Horizontal Time = 520 lines x 26.416us/line = 13.73632ms or 72.8Hz   

//

// Correlation between vertical timing parameters and SED registers   
#define SED_VER_PULSE_WIDTH_CRT 0x02 // VRTC/FPFRAME Pulse Width Register    = Tvp - 1
#define SED_VER_PULSE_START_CRT 0x08 // VRTC/FPFRAME Start Position Register = Tvfp - 1            
#define SED_VER_NONDISP_CRT    0x27 // Vertical Non-Display Period Register = (Tvp + Tvfp + Tvbp) - 1

//

// ***************************************************************************************************   
// DUAL LCD Mode is 640x480 @ 60Hz VGA compatible timing.   PCLK = 25.175Mhz (39.722ns)
// ***************************************************************************************************   
//
//                              LCD MODE HORIZONTAL TIMING PARAMTERS   
//
//                                       |<-------Tha------->|   
//                                       |___________________|                     ______   
// Display Enable   _____________________|                   |____________________|      
//                                       |                   |   
// Horizontal Pulse __           ________|___________________|________          __________   
//                    |_________|        |                   |        |________|   
//                    |<- Thp ->|        |                   |        |   
//                    |         |<-Thbp->|                   |        |   
//                    |                                      |<-Thfp->|   
//                    |<----------------------Tht-------------------->|    
//
// Tha  - Active Display Time                     = 640 pixels   
// Thp  - Horizontal Pulse       = 3.8us/39.72ns  =  96 pixels   
// Thfp - Horizontal Front Porch = .595us/39.72ns =  16 pixels   
// Thbp - Horizontal Backporch   = 1.9us/39.72ns  =  48 pixels   
// Tht  - Total Horizontal Time  =                = 800 pixels @ 39.72ns/pixel = 31.776us or 31.47Khz   

//

// Correlation between horizontal timing parameters and SED registers   
#define SED_HOR_PULSE_WIDTH_LCD 0x0b  // HRTC/FPLINE Pulse Width Register       = (Thp/8) - 1
#define SED_HOR_PULSE_START_LCD  0x02 // HRTC/FPLINE Start Position Register    = (Thfp/8) - 2
#define SED_HOR_NONDISP_LCD   0x13    // Horizontal Non-Display Period Register = ((Thp + Thfp + Thbp)/8) - 1

//
//                              LCD MODE VERTICAL TIMING PARAMTERS   
//
//                                       |<-------Tva------->|   
//                                       |___________________|                     ______   
// Display Enable   _____________________|                   |_____________________|      
//                                       |                   |   
// Vertical Pulse   __           ________|___________________|________          __________   
//                    |_________|        |                   |        |________|   
//                    |<- Tvp ->|        |                   |        |   
//                    |         |<-Tvbp->|                   |        |   
//                    |                                      |<-Tvfp->|   
//                    |<----------------------Tvt-------------------->|    
//
// Tva  - Active Display Time   = 480 lines   
// Tvp  - Vertical Pulse        = 2 lines   
// Tvfp - Vertical Front Porch  = 10 lines   
// Tvbp - Vertical Backporch    = 33 lines   
// Tvt  - Total Horizontal Time = 525 lines @ 31.776us/line = 16.682ms or 60Hz   

//
// Correlation between vertical timing parameters and SED registers   
#define SED_VER_PULSE_WIDTH_LCD 0x01 // VRTC/FPFRAME Pulse Width Register    = Tvp - 1
#define SED_VER_PULSE_START_LCD 0x09 // VRTC/FPFRAME Start Position Register = Tvfp - 1            
#define SED_VER_NONDISP_LCD   0x2c   // Vertical Non-Display Period Register = (Tvp + Tvfp + Tvbp) - 1
