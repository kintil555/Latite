#include "pch.h"
#include "ItemStack.h"

SDK::ItemStack* SDK::ItemStack::constructFromBlock(void* storage, SDK::Block const& block, int count,
                                                   SDK::CompoundTag const* userData) {
    using oFunc_t = ItemStack*(__fastcall*)(void*, Block const*, int, CompoundTag const*);
    auto fn = reinterpret_cast<oFunc_t>(Signatures::ItemStack_ItemStackBlock.result);

    if (!fn) return nullptr;

    const auto item = fn(storage, &block, count, userData);

    if (Signatures::ItemStackVtable.result) {
        item->vtable = reinterpret_cast<void**>(Signatures::ItemStackVtable.result);
    }
#if LATITE_DEBUG
    else {
        Logger::Warn("ItemStackVtable is dead, skipping vtable override (item may be unstable)");
    }
#endif

    return item;
}

void SDK::ItemStack::destruct() {
    using oFunc_t = void(__fastcall*)(ItemStackBase*);
    auto fn = reinterpret_cast<oFunc_t>(Signatures::ItemStackBase_destructor.result);
    if (fn) {
        fn(this);
    }
}
