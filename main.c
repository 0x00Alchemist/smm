#include "stdafx.h"

/*

	20/06/2021 update
	- auto updates beta (scetchy way, can work for some CSGO updates)
	- this doesn't always work at first try, if its not working, pleace do sleep again
*/

CHAR8 *gEfiCallerBaseName = "";
EFI_GUID gEfiSmmBase2ProtocolGuid = { 0xf4ccbfb7, 0xf6e0, 0x47fd, { 0x9d, 0xd4, 0x10, 0xa8, 0xf1, 0x50, 0xc1, 0x91 }};
EFI_GUID gEfiSmmSwDispatch2ProtocolGuid = { 0x18a3c6dc, 0x5eea, 0x48c8, {0xa1, 0xc1, 0xb5, 0x33, 0x89, 0xf9, 0x89, 0x99 }};
EFI_GUID gEfiSmmCpuProtocolGuid = { 0xeb346b97, 0x975f, 0x4a9f, { 0x8b, 0x22, 0xf8, 0xe9, 0x2b, 0xb3, 0xd5, 0x69 }};
/* EFI_GUID gEfiSmmPeriodicTimerDispatch2ProtocolGuid = { 0x4cec368e, 0x8e8e, 0x4d71, {0x8b, 0xe1, 0x95, 0x8c, 0x45, 0xfc, 0x8a, 0x53 }}; */

QWORD gSmmBase2Protocol;
QWORD gSMST;
QWORD gSmmSwDispatch2Protocol;
QWORD map_begin;
QWORD map_end;

#define SW_SMI_VAL 0x56

DWORD crc32(CHAR8 *buf, DWORD len, DWORD init);


EFI_RUNTIME_SERVICES *gRT;
EFI_BOOT_SERVICES    *gBS;
EFI_SYSTEM_TABLE     *gST;
extern DWORD         g_encryption_key ;

/*
        SmmHandler ( ControlSVM from UserMode? ) 
*/

/*
dword_244 =
        0:  b9 15 00 01 c0          mov    ecx,0xc0010015
        5:  0f 32                   rdmsr
        7:  48 c1 e2 20             shl    rdx,0x20
        b:  48 0b c2                or     rax,rdx
        e:  48 83 c8 01             or     rax,0x1
        12: 48 8b d0                mov    rdx,rax
        15: 48 c1 ea 20             shr    rdx,0x20
        19: 0f 30                   wrmsr
        1b: c3                      ret

*/

unsigned __int64 dword_244()
{
        unsigned __int64 result; // rax

        result = __readmsr(0xC0010015) | 1;
        __writemsr(0xC0010015, result);
        return result;
}





/*
        triggered by:
        __outbyte(0xB2, 0x56);
*/
__int64 __fastcall qword_388(__int64 a1, __int64 a2, __int64 a3)
{
        if ( a3 )
        {
                if (*(unsigned char *)(a3 + 8) == SW_SMI_VAL)
                {
                        /*
                        GenerateBeep(5);

                        unsigned __int64 v3 = __readmsr(0xC0010015);
                        if ( !(v3 & 1) )
                        {
                                __writemsr(0xC0010015, v3 | 1);

                                //  gSMST->NumberOfCpus
                                for (QWORD i = 1; i < *(QWORD *)(gSMST + 0x80); ++i )
                                        // SmmStartupThisAp(PVOID Procedure, UINT64 CpuNumber, VOID **ProcArguments)
                                        // gSMST->SmmStartupThisAp(&dword_244, i, 0i64);
                                        (*(void (__fastcall **)(QWORD, unsigned __int64, QWORD))(gSMST + 0x70))((QWORD)dword_244, i, 0i64);

                        }
                        */
                        return 0;
                }
        }
        return 0;
}

BOOLEAN pm_read(QWORD address, VOID *buffer, QWORD length)
{
        if (address < map_begin)
                return 0;
        
        if ((address + length) > map_end)
               return 0;
        
	for (QWORD i = 0; i < length; i++)
		*(unsigned char*)(((QWORD)buffer + i)) = *(unsigned char*)(((QWORD)address + i));

        return 1;
}

unsigned short pm_read_i16(QWORD address)
{
	unsigned short buffer = 0;
	pm_read(address, &buffer, sizeof(buffer));
	return buffer;
}

DWORD pm_read_i32(QWORD address)
{
	DWORD buffer = 0;
	pm_read(address, &buffer, sizeof(buffer));
	return buffer;
}

QWORD pm_read_i64(QWORD address)
{
	QWORD buffer = 0;
	pm_read(address, &buffer, sizeof(buffer));
	return buffer;
}

BOOLEAN pm_write(QWORD address, VOID *buffer, QWORD length)
{
        if (address < map_begin)
                return 0;
        
        if ((address + length) > map_end)
               return 0;
        
	for (QWORD i = 0; i < length; i++)
		*(unsigned char*)(((QWORD)address + i)) = *(unsigned char*)(((QWORD)buffer + i));

        return 1;
}

static QWORD pm_translate(QWORD dir, QWORD va)
{
	QWORD PML4E, PDPTE, PDE, PTE;

	PML4E = pm_read_i64(dir + (unsigned short)((va >> 39) & 0x1FF) * sizeof(QWORD));
	if (PML4E == 0)
		return 0;
	PDPTE = pm_read_i64((PML4E & 0xFFFFFFFFFF000) + (unsigned short)((va >> 30) & 0x1FF) * sizeof(QWORD));
	if (PDPTE == 0)
		return 0;
	if((PDPTE & (1 << 7)) != 0)
		return (PDPTE & 0xFFFFFC0000000) + (va & 0x3FFFFFFF);
	PDE = pm_read_i64((PDPTE & 0xFFFFFFFFFF000) + (unsigned short)((va >> 21) & 0x1FF) * sizeof(QWORD));
	if (PDE == 0)
		return 0;
	if ((PDE & (1 << 7)) != 0)
		return (PDE & 0xFFFFFFFE00000) + (va & 0x1FFFFF);
	PTE = pm_read_i64((PDE & 0xFFFFFFFFFF000) + (unsigned short)((va >> 12) & 0x1FF) * sizeof(QWORD));
	if (PTE == 0)
		return 0;
	return (PTE & 0xFFFFFFFFFF000) + (va & 0xFFF);
}

QWORD vm_get_export_ex(QWORD cr3, BOOLEAN wow64, QWORD module, DWORD crc, DWORD length)
{
	QWORD a0;
	DWORD a1[4], a2[30];

	a0 = module + pm_read_i16(pm_translate(cr3, module + 0x3C));
	a0 = module + pm_read_i32(pm_translate(cr3, a0 + 0x88 - wow64 * 16));
	a1[0]=0;
	pm_read(pm_translate(cr3, a0 + 0x18), &a1[0], 8);
	pm_read(pm_translate(cr3, a0 + 0x20), &a1[2], 8);
	while (a1[0]--) {	
		a0 = pm_read_i32(pm_translate(cr3, module + a1[2] + (a1[0] * 4)));
		pm_read(pm_translate(cr3, module + a0), &a2, sizeof(a2));
		if (crc32((CHAR8*)a2, length, g_encryption_key) == crc)
			return (module + pm_read_i32(pm_translate(cr3, module + a1[1] +
				(pm_read_i16(pm_translate(cr3,module + a1[3] + (a1[0] * 2))) * 4))));
	}
	return 0;
}

QWORD PsInitialSystemProcess;
static DWORD offset_PsGetProcessImageFileName;
static DWORD offset_PsGetProcessExitProcessCalled;
static DWORD offset_ActiveProcessLinks;
static DWORD offset_PsGetProcessWow64Process;
static DWORD offset_PsGetProcessPeb;
static QWORD system_cr3;


static QWORD get_process_by_name(DWORD crc, DWORD length)
{
	QWORD entry;
	char process_name[15]={0};

        QWORD proc = *(QWORD*)(pm_translate(system_cr3, PsInitialSystemProcess));
	entry = proc;
	do {
		pm_read(pm_translate(system_cr3, entry + offset_PsGetProcessImageFileName), process_name, 15);
		process_name[14]=0;
		
		DWORD exitcalled = *(DWORD*)(pm_translate(system_cr3, entry + offset_PsGetProcessExitProcessCalled));
		exitcalled = exitcalled >> 2;
		exitcalled = exitcalled & 1;

		if (!exitcalled && crc32(process_name, length, g_encryption_key) == crc)
			return entry;

		entry = *(QWORD*)(pm_translate(system_cr3, entry + offset_ActiveProcessLinks));

		if (entry == 0)
			break;

		entry = entry - offset_ActiveProcessLinks;


	} while (entry != proc) ;
	return 0;
}



QWORD ntoskrnl;
BOOLEAN gInOs;

QWORD g_process;
QWORD g_process_cr3;
QWORD g_process_peb;
BOOLEAN g_process_wow64;

BOOLEAN vm_read(QWORD address, VOID *buffer, QWORD length)
{
	return pm_read(pm_translate(g_process_cr3, address), buffer, length);
}

BOOLEAN vm_write(QWORD address, VOID *buffer, QWORD length)
{
	return pm_write(pm_translate(g_process_cr3, address), buffer, length);
}

BOOLEAN vm_write_i32(QWORD address, DWORD value)
{
	return vm_write(address, &value, sizeof(value));
}

QWORD vm_read_i64(QWORD address, QWORD length)
{
	QWORD buffer = 0;
	vm_read(address, &buffer, length);
	return buffer;
}


DWORD vm_read_i32(QWORD address)
{
	DWORD buffer = 0;
	vm_read(address, &buffer, 4);
	return buffer;
}

QWORD vm_get_module(DWORD crc, DWORD length)
{
	QWORD peb;
	DWORD a0[5];
	QWORD a1, a2, a3[15];

	if (g_process_wow64) {
		peb = g_process_peb;
		a0[0] = 0x04, a0[1] = 0x0C, a0[2] = 0x14, a0[3] = 0x28, a0[4] = 0x10;
	} else {
		peb = g_process_peb;
		a0[0] = 0x08, a0[1] = 0x18, a0[2] = 0x20, a0[3] = 0x50, a0[4] = 0x20;
	}

	if (peb == 0)
		return 0;

	a1 = vm_read_i64(vm_read_i64(peb + a0[1], a0[0]) + a0[2], a0[0]);

        if (a1 == 0)
                return 0;

	a2 = a2 = vm_read_i64(a1 + a0[0], a0[0]);
        if (a2 == 0)
                return 0;

	while (a1 != a2) {
		vm_read(vm_read_i64(a1 + a0[3], a0[0]), a3, length);

		if (crc32((CHAR8*)a3, length, g_encryption_key) == crc) {
			return vm_read_i64(a1 + a0[4], a0[0]);
		}
		a1 = vm_read_i64(a1, a0[0]);
	}
	return 0;
}

QWORD vm_get_export(QWORD module, DWORD crc, DWORD length)
{
	return vm_get_export_ex(g_process_cr3, g_process_wow64, module, crc, length);
}

BOOLEAN cs_SetConVarInt(DWORD convar, DWORD value)
{
	return vm_write_i32(convar + 0x30, value ^ convar);
}

BOOLEAN cs_SetConVarFloat(DWORD convar, DWORD value)
{
	return vm_write_i32(convar + 0x2C, *(DWORD*)&value ^ convar);
}

BOOLEAN gPatchIsDone;
EFI_HANDLE EfiMainHandlerHandle;

EFI_STATUS EFIAPI EfiMainHandler(
  IN EFI_HANDLE  DispatchHandle,
  IN CONST VOID  *Context         OPTIONAL,
  IN OUT VOID    *CommBuffer      OPTIONAL,
  IN OUT UINTN   *CommBufferSize  OPTIONAL
  )
{

        if (!gInOs) {

                EFI_SMM_CPU_PROTOCOL *SmmCpu = NULL;
                EFI_SMM_SYSTEM_TABLE2 *SMST = (EFI_SMM_SYSTEM_TABLE2 *)gSMST;

                if (EFI_ERROR(SMST->SmmLocateProtocol(&gEfiSmmCpuProtocolGuid, NULL, (VOID **)&SmmCpu)))
                        return 0;

                UINTN cr3;
                if (EFI_ERROR(SmmCpu->ReadSaveState(SmmCpu,
                        sizeof(cr3), EFI_SMM_SAVE_STATE_REGISTER_CR3, SMST->CurrentlyExecutingCpu, (VOID*)&cr3)))
                        return 0;

                if (cr3 == 0)
                        return 0;

                system_cr3 = *(QWORD*)(0x10A0);
                ntoskrnl = *(QWORD*)(0x1070);
                if (system_cr3 == 0)
                        return 0;

                if (system_cr3 != cr3 || ntoskrnl == 0)
                        return 0;

                ntoskrnl = ntoskrnl &~(QWORD)(0xfffff);
                ntoskrnl -= 0x300000;

                if (ntoskrnl == 0xffffffffffd00000)
                        return 0;
                        
                QWORD translate_address = pm_translate(system_cr3, ntoskrnl);
                if (translate_address == 0)
                        return 0;

                if (*(unsigned short*)translate_address != 0x5a4d)
                        return 0;

                PsInitialSystemProcess = vm_get_export_ex(system_cr3, 0, ntoskrnl, 0xf5acb841, 23);
                if (PsInitialSystemProcess == 0)
                        return 0;

                QWORD PsGetProcessId = vm_get_export_ex(system_cr3, 0, ntoskrnl, 0xed22fc88, 15);
                QWORD PsGetProcessExitProcessCalled = vm_get_export_ex(system_cr3, 0, ntoskrnl, 0xc1f02136, 30);
                QWORD PsGetProcessImageFileName = vm_get_export_ex(system_cr3, 0, ntoskrnl, 0x99b8d0bd, 26);
                QWORD PsGetProcessWow64Process = vm_get_export_ex(system_cr3, 0, ntoskrnl, 0xb3862449, 25);
                QWORD PsGetProcessPeb = vm_get_export_ex(system_cr3, 0, ntoskrnl, 0xc4c46f56, 16);

                offset_PsGetProcessExitProcessCalled = *(unsigned int*)(pm_translate(system_cr3, PsGetProcessExitProcessCalled + 2));
                offset_PsGetProcessImageFileName = *(unsigned int*)(pm_translate(system_cr3, PsGetProcessImageFileName + 3));
                offset_ActiveProcessLinks = *(unsigned int*)(pm_translate(system_cr3, PsGetProcessId + 3)) + 8;
                offset_PsGetProcessWow64Process = *(unsigned int*)(pm_translate(system_cr3, PsGetProcessWow64Process + 3));
                offset_PsGetProcessPeb = *(unsigned int*)(pm_translate(system_cr3, PsGetProcessPeb + 3));
                gInOs = 1;
        }

        if (gInOs) {
		if (gPatchIsDone && g_process) {
			if (get_process_by_name(0x567c44c0, 9) == g_process)
				return 0;
			gPatchIsDone = 0;
		}

                g_process = get_process_by_name(0x567c44c0, 9);
                if (g_process == 0)
                        return EFI_SUCCESS;
                
                g_process_peb = pm_read_i64(pm_translate(system_cr3, g_process + offset_PsGetProcessWow64Process));
                g_process_cr3 = pm_read_i64(pm_translate(system_cr3, g_process + 0x28));
                if (g_process_peb) {
                        g_process_peb = pm_read_i64(pm_translate(system_cr3, g_process_peb));
                        g_process_wow64 = 1;
                } else {
			g_process_peb = pm_read_i64(pm_translate(system_cr3, g_process + offset_PsGetProcessPeb));
			g_process_wow64 = 0;
                }

                if (g_process_peb == 0)
                        return EFI_SUCCESS;


		DWORD vstdlib = (DWORD)vm_get_module(0xdffb8bb7, 24);
		if (vstdlib == 0)
			return EFI_SUCCESS;
		
		
		DWORD vstdlib_factory = vstdlib;
		
		vstdlib_factory += 0x1000; // .text begin
		vstdlib_factory += 0x0050; // target function begin
		vstdlib_factory = vstdlib_factory + vm_read_i32(vstdlib_factory + 1) + 5;
		vstdlib_factory += 0xEA;
		vstdlib_factory += 0x01;

		DWORD tmp = vm_read_i32(vstdlib_factory);

		if (vm_read_i32(vm_read_i32(vm_read_i32(tmp + 0x34)) + 0x4)) {
			vstdlib_factory = tmp;
		} else {
			vstdlib_factory += 0x232B;
			vstdlib_factory = vm_read_i32(vstdlib_factory);
			if (vm_read_i32(vm_read_i32(vm_read_i32(vstdlib_factory + 0x34)) + 0x4) == 0)
				vstdlib_factory = vm_read_i32(vstdlib + 0x3310 + 1);
		}

		DWORD convar_factory = vm_read_i32(vstdlib_factory + 0x34);
		convar_factory = vm_read_i32(convar_factory);
		convar_factory = vm_read_i32(convar_factory + 0x4);

		while (convar_factory != 0) {
			CHAR8 buffer[14];
			vm_read(vm_read_i32(convar_factory + 0x0C), (char *)buffer, 14);
			if (crc32((CHAR8*)buffer, 14, g_encryption_key) == 0xb332a0d1)
				break;
			convar_factory = vm_read_i32(convar_factory + 0x4);
		}
		
		if (convar_factory == 0)
			return 0;
		
		cs_SetConVarInt(convar_factory, 1);
		
		gPatchIsDone = 1;
        }
        return EFI_SUCCESS;
}

BOOLEAN GetPhysicalMemoryRanges(QWORD *start_address, QWORD *end_address)
{
	UINT32 version;
	UINTN map_size = 0;
	EFI_MEMORY_DESCRIPTOR *map = 0;
	UINTN map_key;
	UINTN descriptor_size;

	EFI_STATUS status = gBS->GetMemoryMap(&map_size, map, &map_key, &descriptor_size, &version);
	do
	{
		status = gBS->AllocatePool(EfiBootServicesData, map_size, (VOID **)&map);
		if (map == NULL)
		{
			return FALSE;
		}

		status = gBS->GetMemoryMap(&map_size, map, &map_key, &descriptor_size, &version);
		if (EFI_ERROR(status))
		{
			gBS->FreePool(map);
			map = 0;
		}

	} while (status == EFI_BUFFER_TOO_SMALL);

	if (map == 0)
		return FALSE;

	UINT32 descriptor_count = (UINT32)map_size / (UINT32)descriptor_size;

	EFI_MEMORY_DESCRIPTOR *entry_last = (EFI_MEMORY_DESCRIPTOR*)(
		(char *)map + (  (descriptor_count - 1) * descriptor_size)
		);

	if (start_address)
		*start_address = 0x1000; // windows memory map always starts from 0x1000

	if (end_address)
		*end_address = entry_last->PhysicalStart + EFI_PAGES_TO_SIZE(entry_last->NumberOfPages);

	gBS->FreePool(map);

	return TRUE;
}


EFI_STATUS EFIAPI EfiMain(IN EFI_LOADED_IMAGE *LoadedImage, IN EFI_SYSTEM_TABLE *SystemTable)
{
	gRT = SystemTable->RuntimeServices;
	gBS = SystemTable->BootServices;
	gST = SystemTable;

	if (!GetPhysicalMemoryRanges(&map_begin, &map_end))
		return 0;
	
	if (EFI_ERROR(gBS->LocateProtocol(&gEfiSmmBase2ProtocolGuid, 0, (void **)&gSmmBase2Protocol)))
		return 0;


        /*
        * gSmmBase2Protocol->GetSmstLocation(
        *      gSmmBase2Protocol,
        *      (EFI_SMM_SYSTEM_TABLE2 *)&gSMST)
        */
	(*(void (__fastcall **)(__int64, __int64 *))(gSmmBase2Protocol + 0x08))(gSmmBase2Protocol, &gSMST);
	if (gSMST == 0)
		return 0;

        /* gSMST->SmmLocateProtocol(
         *      &gEfiSmmSwDispatch2ProtocolGuid,
         *      0,
         *      (EFI_SMM_SW_DISPATCH2_PROTOCOL *)&gSmmSwDispatch2Protocol)
         */
	if (EFI_ERROR((*(EFI_STATUS (__fastcall **)(EFI_GUID *, QWORD, __int64 *))(gSMST + 0x0D0))(
		&gEfiSmmSwDispatch2ProtocolGuid,
		0,
		(__int64 *)&gSmmSwDispatch2Protocol)))
		return 0;

        /*
         *  gSmmSwDispatch2Protocol->Register(gSmmSwDispatch2Protocol,
         *          qword_388, 
         *          &SW_SMI_VAL (0x56),
         *          &v24 (0x00));
        */

	
	QWORD val = SW_SMI_VAL;
	QWORD buf = 0;
	(*(__int64 (__fastcall **)(__int64, __int64 *, __int64 *, __int64 *))gSmmSwDispatch2Protocol)(
		gSmmSwDispatch2Protocol,
		(__int64 *)qword_388,
		&val,
		&buf);
	

        struct _EFI_SMM_SYSTEM_TABLE2 *tbl = (struct _EFI_SMM_SYSTEM_TABLE2*)gSMST;
        tbl->SmiHandlerRegister(EfiMainHandler, 0, &EfiMainHandlerHandle);

	return 0;
}


/*
0: kd> dt demo!EFI_SMM_SYSTEM_TABLE2 0`887f9730
   +0x000 Hdr : EFI_TABLE_HEADER
   +0x018 SmmFirmwareVendor : (null) 
   +0x020 SmmFirmwareRevision : 0
   +0x028 SmmInstallConfigurationTable : 0x00000000`887fa1b0 Void
   +0x030 SmmIo : EFI_SMM_CPU_IO2_PROTOCOL
   +0x050 SmmAllocatePool : 0x00000000`887fb61c Void
   +0x058 SmmFreePool : 0x00000000`887fb744 Void
   +0x060 SmmAllocatePages : 0x00000000`887fbd20 Void
   +0x068 SmmFreePages : 0x00000000`887fbe30 Void
   +0x070 SmmStartupThisAp : 0x00000000`887e0af0 Void
   +0x078 CurrentlyExecutingCpu : 0
   +0x080 NumberOfCpus : 4
   +0x088 CpuSaveStateSize : 0x00000000`887ddd50 -> 0x400
   +0x090 CpuSaveState : 0x00000000`887ddf50 -> 0x00000000`887dac00 Void
   +0x098 NumberOfTableEntries : 6
   +0x0a0 SmmConfigurationTable : 0x00000000`887e5810 Void
   +0x0a8 SmmInstallProtocolInterface : 0x00000000`887fb928 Void
   +0x0b0 SmmUninstallProtocolInterface : 0x00000000`887fbaf4 Void
   +0x0b8 SmmHandleProtocol : 0x00000000`887fbc1c Void
   +0x0c0 SmmRegisterProtocolNotify : 0x00000000`887fbf2c Void
   +0x0c8 SmmLocateHandle : 0x00000000`887fa058 Void
   +0x0d0 SmmLocateProtocol : 0x00000000`887f9f8c Void
   +0x0d8 SmiManage : 0x00000000`887fb2fc Void
   +0x0e0 SmiHandlerRegister : 0x00000000`887fb3d4 Void
   +0x0e8 SmiHandlerUnRegister : 0x00000000`887fb48c Void
*/


/*
PLAN: 

gEfiSmmPeriodicTimerDispatch2ProtocolGuid = { 0x4cec368e, 0x8e8e, 0x4d71, {0x8b, 0xe1, 0x95, 0x8c, 0x45, 0xfc, 0x8a, 0x53 }}
Status = gSMST->SmmLocateProtocol (
	&gEfiSmmPeriodicTimerDispatch2ProtocolGuid,
	NULL,
	(VOID **)&gSmmPeriodicTimerDispatch2
	);


EFI_SMM_PERIODIC_TIMER_REGISTER_CONTEXT m_PeriodicTimerDispatch2RegCtx = { 1000000, 640000 };
Status = gSmmPeriodicTimerDispatch2->Register(
	gSmmPeriodicTimerDispatch2, 
	PeriodicTimerDispatch2Handler, 
	&m_PeriodicTimerDispatch2RegCtx,
	DispatchHandle
	);


dword_244=0x320

__int64 __fastcall qword_388(__int64 a1, __int64 a2, __int64 a3)
{
  unsigned __int64 v3; // rax
  unsigned __int64 i; // rbx

  if ( a3 )
  {
    if ( *(_BYTE *)(a3 + 8) == 0x56 )
    {
      v3 = __readmsr(0xC0010015);
      if ( !(v3 & 1) )
      {
        __writemsr(0xC0010015, v3 | 1);
        for ( i = 1i64; i < *(_QWORD *)(qword_520 + 128); ++i )
          (*(void (__fastcall **)(int *, unsigned __int64, _QWORD))(qword_520 + 112))(&dword_244, i, 0i64);
      }
    }
  }
  return 0i64;
}



*/

