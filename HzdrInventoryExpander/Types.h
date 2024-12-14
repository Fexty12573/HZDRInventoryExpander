#pragma once

struct InventoryCapacityModificationComponentResource {
    void* vft;
    GUID guid;
    int64_t unk;
    const char* name;
    int weapon_capacity_increase;     // 0x28 (40)
    int tool_capacity_increase;       // 0x2C (44)
    int unk2;                         // 0x30 (48)
    int mod_capacity_increase;        // 0x34 (52)
    int outfit_capacity_increase;     // 0x38 (56)
    int resource_capacity_increase;   // 0x3C (60)
};

struct InventoryCapacityModificationComponent {
    void* vft;
    unsigned char unk[0x28];
    InventoryCapacityModificationComponentResource* resource;
};

struct InventoryCapacityComponentResource {
    void* vft;
    GUID guid;
    int64_t unk;
    const char* name;
    int weapon_capacity;     // 0x28 (40)
    int tool_capacity;       // 0x2C (44)
    int unk2;                // 0x30 (48)
    int mod_capacity;        // 0x34 (52)
    int outfit_capacity;     // 0x38 (56)
    int resource_capacity;   // 0x3C (60)
};

struct InventoryCapacityComponent {
    void* vft;
    unsigned char unk[0x28];
    InventoryCapacityComponentResource* resource;
};
