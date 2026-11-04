import ctypes, time
ole32 = ctypes.windll.ole32
shell32 = ctypes.windll.shell32
user32 = ctypes.windll.user32
ole32.OleInitialize(None)

pDesktop = ctypes.c_void_p()
shell32.SHGetDesktopFolder.argtypes = [ctypes.POINTER(ctypes.c_void_p)]
shell32.SHGetDesktopFolder(ctypes.byref(pDesktop))
pVtable = ctypes.cast(pDesktop.value, ctypes.POINTER(ctypes.c_void_p))
vtable = ctypes.cast(pVtable.contents.value, ctypes.POINTER(ctypes.c_void_p))

ParseDisplayNameFunc = ctypes.WINFUNCTYPE(ctypes.c_long, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_ulong), ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_ulong))(vtable[3])
GetUIObjectOfFunc = ctypes.WINFUNCTYPE(ctypes.c_long, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint, ctypes.POINTER(ctypes.c_void_p), ctypes.c_void_p, ctypes.c_void_p, ctypes.POINTER(ctypes.c_void_p))(vtable[10])

pidl = ctypes.c_void_p()
chEaten = ctypes.c_ulong(0)
attr = ctypes.c_ulong(0)
path = "C:\\Windows"
ParseDisplayNameFunc(pDesktop, None, None, path, ctypes.byref(chEaten), ctypes.byref(pidl), ctypes.byref(attr))

IID_IContextMenu = (ctypes.c_byte * 16)(*bytes.fromhex('e414020000000000c000000000000046'))
pMenu = ctypes.c_void_p()
GetUIObjectOfFunc(pDesktop, None, 1, ctypes.byref(pidl), ctypes.byref(IID_IContextMenu), None, ctypes.byref(pMenu))

pVtableMenu = ctypes.cast(pMenu.value, ctypes.POINTER(ctypes.c_void_p))
vtableMenu = ctypes.cast(pVtableMenu.contents.value, ctypes.POINTER(ctypes.c_void_p))
QueryContextMenuFunc = ctypes.WINFUNCTYPE(ctypes.c_long, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_uint, ctypes.c_uint, ctypes.c_uint, ctypes.c_uint)(vtableMenu[3])

for flag_name, flag_val in [
    ("CMF_NORMAL", 0),
    ("CMF_CANRENAME", 0x10),
    ("CMF_ASYNCVERBSTATE | CMF_CANRENAME", 0x400 | 0x10),
    ("CMF_ASYNCVERBSTATE | CMF_CANRENAME | CMF_OPTIMIZEFORINVOKE", 0x400 | 0x10 | 0x800),
]:
    hMenu = user32.CreatePopupMenu()
    t0 = time.time()
    hr = QueryContextMenuFunc(pMenu, hMenu, 0, 1, 0x6fff, flag_val)
    t1 = time.time()
    count = user32.GetMenuItemCount(hMenu)
    print(f"{flag_name}: {(t1 - t0)*1000:.2f} ms, count={count}")
    user32.DestroyMenu(hMenu)
