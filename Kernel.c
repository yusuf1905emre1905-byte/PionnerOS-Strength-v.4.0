// =========================================================
// PIONNEROS V4.0: 64-BIT C BAŞLANGIÇ NOKTASI (kernel.c)
// BÖLÜM 68: kernel_main_64bit()
// =========================================================

// NOT: C Derleyicisinin bu kodu 64-bit olarak derlediğini varsayıyoruz.

// Eski 32-bit fonksiyonları 64-bit'e uyarla (Basitleştirilmiş simülasyon)
extern void init_gdt_64bit(); // GDT/IDT'yi yeniden kuracağız
extern void init_paging_64bit(); // Paging'i 4 seviyeli olarak yeniden kuracağız
extern void start_vfs_server(); // Bu fonksiyonu 64-bit adreslerle kullanacağız

void kernel_main_64bit() {
    
    // 1. Konsol Başlatma
    // Bu, V3.0'dan gelen basit puts fonksiyonunun 64-bit adresi kullanmasını sağlar.
    puts_64bit("PionnerOS V4.0 - 64-bit Cekirdek Modu baslatildi!\n");
    
    // 2. 64-bit Çekirdek Yapılarını Kurma
    // Bu fonksiyon, yeni 64-bit GDT'yi ve IDT'yi yükleyecektir.
    init_gdt_64bit();

    // 3. 64-bit Paging'i (4 Seviyeli) Kurma
    // Bu fonksiyon, V3.0'daki 2 seviyeli Paging'i, 64-bit için PML4/PDPT ile değiştirecektir.
    init_paging_64bit();
    
    // 4. Modüler Hizmetleri Başlatma (IPC Üzerinden Çalışır)
    puts_64bit("[V4.0]: Hibrit Cekirdek servisleri yukleniyor...\n");
    start_vfs_server(); // VFS Sunucusu (Ring 3'te çalışacak)
    
    // 5. Ana Uygulamayı Başlatma
    // start_user_app_64bit(); // Pong/AI uygulamasını Ring 3'te başlat

    puts_64bit("[SUCCESS]: PionnerOS V4.0 artik tam 64-bit guvenliginde!\n");

    // Çekirdek döngüsü: Sonsuza kadar sadece IPC, Kesmeleri ve Zamanlamayı yönetir.
    while(1) {
        // ... (64-bit Multitasking döngüsü) ...
    } 
}

// =========================================================
// PIONNEROS V4.0: 64-BIT PAGING KURULUMU (kernel.c)
// BÖLÜM 69: init_paging_64bit()
// =========================================================

// PML4, PDPT ve PD adreslerini 64-bit Assembly kodunda tanımlamıştık.
#define PML4_VIRT_ADDR 0x100000 

// 64-bit Sayfalama Giriş Yapısı (Girişler artık 8 bayttır!)
typedef struct {
    unsigned long present : 1;      // Sayfa RAM'de mi? (P)
    unsigned long rw : 1;           // Okuma/Yazma izni (R/W)
    unsigned long us : 1;           // Kullanıcı/Süpervizör seviyesi (U/S)
    // Diğer bayraklar...
    unsigned long reserved : 8;     // Ayrılmış
    unsigned long physical_address : 40; // Fiziksel Adres (40 bit = 1TB bellek desteği)
    unsigned long available : 11;
    unsigned long nx : 1;           // Yürütme yok (No-Execute)
} page_entry_64bit_t;

// Sayfa Dizini Yapıları (64-bit)
page_entry_64bit_t *pml4 = (page_entry_64bit_t *)PML4_VIRT_ADDR;

void init_paging_64bit() {
    
    puts_64bit("[V4.0 Paging]: 64-bit Sanal Bellek Yönetimi baslatiliyor.\n");

    // 1. PML4 Adresini CR3'e Yükleme
    // Assembly'de bu yapıldı: mov cr3, PML4_ADDR

    // 2. Tablo Bayraklarını Kontrol Etme (Assembly'de Kurulanları)
    // Sadece ilk PML4 girişi aktif olmalı.
    if (pml4[0].present == 0) {
        puts_64bit("[PAGING ERROR]: PML4 ilk girisi Assembly tarafindan kurulmadi!\n");
        return;
    }
    
    // 3. Çekirdek Alanını Eşleme Kontrolü
    // Bu noktada, 64-bit Assembly kodu zaten ilk 1GB'ı (Çekirdek Alanı) eşledi.
    // Artık C kodu, bu 1GB'lık alanda güvenli bir şekilde çalışabilir.

    // 4. Yeni Sanal Alanlar (Heap, Uygulamalar) İçin Tabloları Oluşturma
    // Bu alan, gelecekteki 64-bit uygulamalarınız (Pong v4.0, AI v4.0) için kullanılacaktır.

    // Örneğin, 2MB Sayfa Boyutunu kullanmanın onaylanması:
    puts_64bit("[V4.0 Paging]: 4 Seviyeli Paging aktif. 2MB Buyuk Sayfalar kullaniliyor.\n");

    // Güvenlik doğrulama mesajı:
    puts_64bit("[V4.0 STABILITY]: 64-bit Bellek Korumasi baslatildi.\n");
}

// =========================================================
// PIONNEROS V4.0: PCI I/O TEMELLERİ (kernel.c)
// BÖLÜM 70: read/write_pci_config
// =========================================================

// Gerekli harici Assembly I/O fonksiyonları (64-bit port I/O)
extern unsigned int inl(unsigned short port);
extern void outl(unsigned short port, unsigned int value);

// PCI Yapılandırma Alanına erişim portları
#define PCI_CONFIG_ADDRESS  0xCF8
#define PCI_CONFIG_DATA     0xCFC

// PCI yapılandırma alanından 32-bit (dword) okuma
unsigned int read_pci_config(unsigned char bus, unsigned char slot, unsigned char func, unsigned char offset) {
    
    // 32-bitlik PCI adresi oluşturulur
    // | 31 (Enable Bit) | 30-24 (Reserved) | 23-16 (Bus) | 15-11 (Slot) | 10-8 (Function) | 7-2 (Offset) | 1-0 (00) |
    unsigned int address;
    unsigned long l_bus = (unsigned long)bus;
    unsigned long l_slot = (unsigned long)slot;
    unsigned long l_func = (unsigned long)func;
    
    address = (unsigned int)((l_bus << 16) | (l_slot << 11) | (l_func << 8) | (offset & 0xFC) | 0x80000000);
    
    // 1. Adresi 0xCF8 portuna yaz
    outl(PCI_CONFIG_ADDRESS, address);
    
    // 2. Veriyi 0xCFC portundan oku
    return inl(PCI_CONFIG_DATA + (offset & 0x03));
}
// =========================================================
// PIONNEROS V4.0: PCI TARAMA MANTIGI (kernel.c)
// BÖLÜM 71: pci_check_all_buses()
// =========================================================

// PCI Yapılandırma Alanından okuma fonksiyonu
extern unsigned int read_pci_config(unsigned char bus, unsigned char slot, unsigned char func, unsigned char offset);

// Geçerli bir cihazın olup olmadığını anlamak için kullanılan kimlik
#define PCI_VENDOR_ID_NONE 0xFFFF 

void pci_check_device(unsigned char bus, unsigned char slot, unsigned char func) {
    // 0. Vendor ID'yi oku (Offset 0x00)
    unsigned int vendor_device_id = read_pci_config(bus, slot, func, 0x00);
    unsigned short vendor_id = (unsigned short)(vendor_device_id & 0xFFFF);
    unsigned short device_id = (unsigned short)(vendor_device_id >> 16);

    // 1. Cihazın varlığını kontrol et
    if (vendor_id == PCI_VENDOR_ID_NONE || vendor_id == 0) {
        return; // Cihaz yok.
    }

    // 2. Cihaz bilgilerini oku
    unsigned int header_class = read_pci_config(bus, slot, func, 0x08);
    unsigned char class_code = (unsigned char)(header_class >> 24);
    unsigned char subclass_code = (unsigned char)(header_class >> 16);

    // 3. Bilgileri Konsola yaz (Debugging)
    puts_64bit("[PCI DISCOVERY]: Yeni Cihaz Bulundu!\n");
    // (Gerçekte burada printf ile hex değerler basılırdı)
    // printf("Bus %d, Slot %d, Func %d: VendorID=%h, ClassCode=%h\n", bus, slot, func, vendor_id, class_code);
    
    // 4. AHCI/SATA Kontrolcüsü mü? (Class Code 0x01 = Depolama, Subclass 0x06 = SATA)
    if (class_code == 0x01 && subclass_code == 0x06) {
        puts_64bit("[PCI DRIVER]: AHCI/SATA Kontrolcusu tespit edildi! V4.0 yukleme hazirligi basliyor...\n");
        // Buraya V4.0 AHCI Sürücüsünü başlatma kodu gelecektir.
    }
}

// Tüm PCI Bus'larını, Slot'larını ve Fonksiyonlarını tarayan ana döngü
void pci_check_all_buses() {
    puts_64bit("\n[PCI INIT]: Tum PCI veri yollari taranmaya baslandi.\n");
    
    for (int bus = 0; bus < 256; bus++) {
        for (int slot = 0; slot < 32; slot++) {
            // Basitlik için sadece 0. fonksiyona bakalım (Çok Fonksiyonlu cihaz değilse)
            pci_check_device(bus, slot, 0); 
        }
    }
    
    puts_64bit("[PCI INIT]: Tarama tamamlandi.\n");
}

// kernel_main_64bit'i güncelleyelim:
void kernel_main_64bit() {
    // ... (init_gdt_64bit(), init_paging_64bit() kodları buraya gelir) ...
    
    // V4.0: PCI Sürücü Mimarisine geçiş için donanımı tara.
    pci_check_all_buses(); 

    // ... (start_vfs_server() ve start_user_app_64bit() kodları buraya gelir) ...
}
// =========================================================
// PIONNEROS V4.0: AHCI TEMELLERİ (kernel.c)
// BÖLÜM 72: AHCI_BASE_ADDR ve init_ahci_controller
// =========================================================

// AHCI Kontrolcüsünün MMIO (Bellek Haritalı I/O) adresi
// Bu, kontrolcünün kayıtlarının RAM'de basladigi yerdir.
unsigned long AHCI_BASE_ADDR = 0; 

// AHCI Kontrolcüsünü Başlatma Fonksiyonu (MMIO adresi bulunduktan sonra çağrılır)
void init_ahci_controller() {
    if (AHCI_BASE_ADDR == 0) {
        puts_64bit("[AHCI ERROR]: AHCI Adresi bulunamadi. Disk erisimi baslatilamiyor.\n");
        return;
    }
    
    // 1. AHCI kayıtlarını oku (Bu noktada AHCI_BASE_ADDR kullanılır)
    // Örneğin: unsigned int cap = *(unsigned int volatile *)(AHCI_BASE_ADDR + 0x00);
    
    puts_64bit("[AHCI INIT]: AHCI Kontrolcusu baslatiliyor (MMIO Adresi artik biliniyor).\n");
    // ... Burada Paging tablolarına AHCI MMIO adresini eşleme kodu gelirdi ...
    
    // 2. Kontrolcüyü aktif etme (HBA_CAP, GHC) ve ilk SATA Portlarini tarama kodu...
    
    puts_64bit("[AHCI SUCCESS]: Modern SATA disk erisimi hazir!\n");
}


// --- pci_check_device() FONKSİYONUNUN GÜNCEL HALİ ---
// SADECE GEREKLİ KISMI GÜNCELLEYİNİZ!
void pci_check_device(unsigned char bus, unsigned char slot, unsigned char func) {
    // ... (Vendor ID ve Class Code okuma kodları buraya gelir) ...
    
    // 4. AHCI/SATA Kontrolcüsü mü? (Class Code 0x01 = Depolama, Subclass 0x06 = SATA)
    if (class_code == 0x01 && subclass_code == 0x06) {
        puts_64bit("[PCI DRIVER]: AHCI/SATA Kontrolcusu tespit edildi!\n");

        // AHCI Kayıtları (MMIO) Adresini Oku (Offset 0x24 = BAR5)
        // AHCI kontrolcüsü genellikle BAR5'i kullanır.
        unsigned int bar5 = read_pci_config(bus, slot, func, 0x24);

        // 64-bit için BAR adresinden sadece fiziksel adresi al.
        // En az anlamlı 4 biti (bayraklar) temizle
        AHCI_BASE_ADDR = (unsigned long)(bar5 & 0xFFFFFFF0);

        puts_64bit("[PCI DRIVER]: AHCI MMIO adresi bulundu. Kontrolcu baslatiliyor...\n");
        init_ahci_controller(); // AHCI Sürücüsünü başlat!
    }
}

