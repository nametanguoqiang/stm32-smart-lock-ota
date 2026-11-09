#include "ota_meta.h"                                           // 本模块的结构体和地址宏
#include "crc32.h"                                              // crc32_calc，算自校验用
#include "stm32f1xx_hal.h"                                      // HAL_FLASH_* 系列函数

#define META_CRC_LEN   (sizeof(ota_meta_t) - sizeof(uint32_t))  // 参与 CRC 的长度：20 - 4 = 16 字节

void ota_meta_load(ota_meta_t *m)                               // 从 Flash 把元数据读进内存
{
    const ota_meta_t *p = (const ota_meta_t *)OTA_META_ADDR;    // 把固定地址强转成结构体指针
    *m = *p;                                                    // 整体拷贝，Flash 可以直接读，不需要解锁
}

int ota_meta_is_valid(const ota_meta_t *m)                      // 判断元数据可不可信，可信返回 1
{
    uint32_t crc;                                               // 用来装重新算出来的 CRC

    if (m->magic != OTA_META_MAGIC)   return 0;                 // 第一道：魔数不对，说明从没初始化过

    crc = crc32_calc((const uint8_t *)m, META_CRC_LEN);         // 按前 16 字节重算 CRC
    if (crc != m->crc_meta)           return 0;                 // 第二道：CRC 对不上，说明被写坏了

    if (m->active_slot > 1)           return 0;                 // 第三道：槽号只能是 0 或 1
    return 1;                                                   // 三道都过，可信
}

void ota_meta_set_default(ota_meta_t *m)                        // 第一次上电时填一套出厂默认值
{
    m->magic       = OTA_META_MAGIC;                            // 魔数写上，表示这块区域初始化过了
    m->active_slot = OTA_SLOT_A;                                // 默认启动 A 槽
    m->trial       = 0;                                         // 不是试启动状态
    m->boot_count  = 0;                                         // 失败次数清零
    m->crc_a       = 0;                                         // A 槽还没算过 CRC
    m->crc_b       = 0;                                         // B 槽还没算过 CRC
    m->crc_meta    = crc32_calc((const uint8_t *)m, META_CRC_LEN);  // 最后算自校验，前面字段没填完不能算
}

void ota_meta_save(const ota_meta_t *m)                         // 把元数据写回 Flash
{
    ota_meta_t tmp = *m;                                        // 先拷一份，不动调用者传进来的数据
    tmp.crc_meta = crc32_calc((const uint8_t *)&tmp, META_CRC_LEN);  // 按最终要写的内容重算自校验

    FLASH_EraseInitTypeDef erase;                               // 擦除参数结构体
    uint32_t page_err = 0;                                      // 擦除失败时，这里回填出错的页号

    HAL_FLASH_Unlock();                                         // 解锁 Flash 控制寄存器，不解锁写不进去

    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_PGERR | FLASH_FLAG_WRPERR);  // 清掉上一轮残留的错误标志

    erase.TypeErase   = FLASH_TYPEERASE_PAGES;                  // 按页擦，不是整片擦
    erase.PageAddress = OTA_META_ADDR;                          // 擦元数据所在那一页
    erase.NbPages     = 1;                                      // 只擦一页（2KB）

    if (HAL_FLASHEx_Erase(&erase, &page_err) != HAL_OK)         // 擦除失败就退出
    {
        HAL_FLASH_Lock();                                       // 失败也要锁回去，不然 Flash 一直开着
        return;                                                 // 元数据没写成，调用者要自己处理
    }

    const uint32_t *src = (const uint32_t *)&tmp;               // 把结构体当成一串 32 位字
    for (uint32_t i = 0; i < sizeof(ota_meta_t) / 4; i++)       // 20 字节 / 4 = 5 次
    {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,           // 一次写 32 位
                              OTA_META_ADDR + i * 4,            // 第 i 个字要写到哪个地址
                              src[i]) != HAL_OK)                // 要写的内容
        {
            HAL_FLASH_Lock();                                   // 写失败就停，别继续写
            return;                                             // 后面的字也不用写了
        }
    }

    HAL_FLASH_Lock();                                           // 全部写完，锁回去防止误写
}

uint32_t ota_slot_addr(uint8_t slot)                            // 槽号转起始地址
{
    return (slot == OTA_SLOT_A) ? OTA_SLOT_A_ADDR : OTA_SLOT_B_ADDR;  // 0 返回 A 的地址，其它返回 B 的地址
}
