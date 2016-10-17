#include "global.h"

#define WRITEU8(addr, data) *(vu8*)(addr) = data
#define WRITEU16(addr, data) *(vu16*)(addr) = data
#define WRITEU32(addr, data) *(vu32*)(addr) = data

u32 threadStack[0x1000];
Handle fsUserHandle;
FS_archive sdmcArchive;


#define IO_BASE_PAD		1
#define IO_BASE_LCD		2
#define IO_BASE_PDC		3
#define IO_BASE_GSPHEAP		4

u32 IoBasePad = 0xFFFD4000;

u32 getKey() {
	return (*(vu32*)(IoBasePad) ^ 0xFFF) & 0xFFF;
}

void waitKeyUp() {
	while (getKey() != 0) {
		svc_sleepThread(100000000);
	}
}

u8 cheatEnabled[64];
int isNewNtr = 0;


u32 plgGetIoBase(u32 IoType);
GAME_PLUGIN_MENU gamePluginMenu;

void initMenu() {
	memset(&gamePluginMenu, 0, sizeof(GAME_PLUGIN_MENU));
}

void addMenuEntry(u8* str) {
	if (gamePluginMenu.count > 64) {
		return;
	}
	u32 pos = gamePluginMenu.count;
	u32 len = strlen(str) + 1;
	gamePluginMenu.offsetInBuffer[pos] = gamePluginMenu.bufOffset;
	strcpy(&(gamePluginMenu.buf[gamePluginMenu.bufOffset]), str);
	
	gamePluginMenu.count += 1;
	gamePluginMenu.bufOffset += len;
}

u32 updateMenu() {
	PLGLOADER_INFO *plgLoaderInfo = (void*)0x07000000;
	plgLoaderInfo->gamePluginPid = getCurrentProcessId();
	plgLoaderInfo->gamePluginMenuAddr = (u32)&gamePluginMenu;
	
	u32 ret = 0;
	u32 hProcess;
	u32 homeMenuPid = plgGetIoBase(5);
	if (homeMenuPid == 0) {
		return 1;
	}
	ret = svc_openProcess(&hProcess, homeMenuPid);
	if (ret != 0) {
		return ret;
	}
	copyRemoteMemory( hProcess, &(plgLoaderInfo->gamePluginPid), CURRENT_PROCESS_HANDLE,  &(plgLoaderInfo->gamePluginPid), 8);
	final:
	svc_closeHandle(hProcess);
	return ret;
}

int scanMenu() {
	u32 i;
	for (i = 0; i < gamePluginMenu.count; i++) {
		if (gamePluginMenu.state[i]) {
			gamePluginMenu.state[i] = 0;
			return i;
		}
	}
	return -1;
}

// detect language (0: english, 1: chinese)
int detectLanguage() {
	if(plgGetIoBase(6) == 2052)
		return 1; //CN
	return 0; //EN
}

// add one cheat menu entry
void addCheatMenuEntry(u8* str) {
	u8 buf[100];
	xsprintf(buf, "[ ] %s", str);
	addMenuEntry(buf);
}

// this function will be called when the state of cheat item changed
void onCheatItemChanged(int id, int enable) {
	// TODO: handle on cheat item is select or unselected
	cheatEnabled[(u8)(id)] = enable;
}

// freeze the value
void freezeCheatValue() {
    u32 key,pointer;
	if (cheatEnabled[0]) {
		WRITEU8(0x08c84374, 0xA);//0x08C83D94 for 31.6M;0x08c84274 for 92.7M;0x08c84374 for 95.5MB
	}
	if (cheatEnabled[1]) {
		WRITEU16(0x08C6FF55, 0x0101);//0x08C6F975 for 31.6M;0x08C6FE55 for 92.7M;0x08C6FF55 for 95.5MB
	}
	if (cheatEnabled[2]) {//TODO: need to be a function
        key = getKey();
        if (key == BUTTON_SE & BUTTON_DU) {
            // toggle cheats when SELECT button pressed
            cheatEnabled[2] = 1;
            // wait until key is up
            waitKeyUp();
        }
        if (cheatEnabled[2]) {
			pointer = *(vu32*)(0x81FB510);
			if(pointer != 0x5D48C0 && 0x8000000 < pointer && pointer < 0x8DEFFFF){//magic number
				WRITEU8(pointer + 0xB2,0x1);
			}
        }
    }	
	// TODO: handle your own cheat items
}

// update the menu status
void updateCheatEnableDisplay(id) {
	gamePluginMenu.buf[gamePluginMenu.offsetInBuffer[id] + 1] = cheatEnabled[id] ? 'X' : ' ';
}

// scan and handle events
void scanCheatMenu() {
	int ret = scanMenu();
	if (ret != -1) {
		cheatEnabled[ret] = !cheatEnabled[ret];
		updateCheatEnableDisplay(ret);
		onCheatItemChanged(ret, cheatEnabled[ret]);
	}
}

// init 
void initENCheatMenu() {
	initMenu();
	addCheatMenuEntry("O-Power Always Full");
	addCheatMenuEntry("Quick hatching");
	addCheatMenuEntry("100% Catch rate");
	updateMenu();
}

void initCNCheatMenu() {
	initMenu();
	addCheatMenuEntry("O-Power不减");
	addCheatMenuEntry("快速孵蛋");
	addCheatMenuEntry("100%抓捕");
	updateMenu();
}

void gamePluginEntry() {
	u32 ret, key;
	INIT_SHARED_FUNC(plgGetIoBase, 8);
	INIT_SHARED_FUNC(copyRemoteMemory, 9);
	// wait for game starts up (5 seconds)
	svc_sleepThread(5000000000);

	if (((NS_CONFIG*)(NS_CONFIGURE_ADDR))->sharedFunc[8]) {
		isNewNtr = 1;
	} else {
		isNewNtr = 0;
	}
	
	if (isNewNtr) {
		IoBasePad = plgGetIoBase(IO_BASE_PAD);
	}
	if (1 == detectLanguage()){
		initCNCheatMenu();// cn
	} else {
		initENCheatMenu(); // en
	}
	
	while (1) {
		svc_sleepThread(100000000);
		scanCheatMenu();
		freezeCheatValue();
	}
}
