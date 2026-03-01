extern unsigned int _kernel_end;
unsigned int placement_address = (unsigned int)&_kernel_end;

void* kmalloc(unsigned int size) {
    unsigned int tmp = placement_address;
    placement_address += size;
    
    // Belleði 4-byte hizalayalým (Hýz için kritik)
    if (placement_address & 0xFFFFFFFC) {
        placement_address &= 0xFFFFFFFC;
        placement_address += 0x4;
    }
    
    return (void*)tmp;
}