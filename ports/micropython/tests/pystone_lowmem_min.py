# Pystone 1.2 (slice-free copy for the minimal image: list(x) where the original writes x[:])
# Pystone 1.2 (Dhrystone in Python), low-memory form for a MicroPython prompt.
# Same operation mix as the standard pystone.py; records are lists, not classes, to keep the heap small.
# Prints pystones per second for LOOPS passes timed with time.ticks_ms.
import time

LOOPS = 2000
Ident1, Ident2, Ident3, Ident4, Ident5 = 1, 2, 3, 4, 5
PtrGlbNext = PtrGlb = None
Char1Glob = Char2Glob = ''
Array1Glob = [0] * 51
Array2Glob = [list(x) for x in [Array1Glob] * 51]
IntGlob = 0
BoolGlob = 0

def Proc0(loops):
    global IntGlob, BoolGlob, Char1Glob, Char2Glob, Array1Glob, Array2Glob, PtrGlb, PtrGlbNext
    PtrGlbNext = [None, 0, 0, 0, "", 0]
    PtrGlb = [PtrGlbNext, Ident1, 0, 40, "DHRYSTONE PROGRAM, SOME STRING", 0]
    String1Loc = "DHRYSTONE PROGRAM, 1'ST STRING"
    Array2Glob[8][7] = 10
    t0 = time.ticks_ms()
    for i in range(loops):
        Proc5()
        Proc4()
        IntLoc1 = 2
        IntLoc2 = 3
        String2Loc = "DHRYSTONE PROGRAM, 2'ND STRING"
        EnumLoc = Ident2
        BoolGlob = not Func2(String1Loc, String2Loc)
        while IntLoc1 < IntLoc2:
            IntLoc3 = 5 * IntLoc1 - IntLoc2
            IntLoc3 = Proc7(IntLoc1, IntLoc2)
            IntLoc1 = IntLoc1 + 1
        Proc8(Array1Glob, Array2Glob, IntLoc1, IntLoc3)
        PtrGlb = Proc1(PtrGlb)
        CharIndex = 'A'
        while CharIndex <= Char2Glob:
            if EnumLoc == Func1(CharIndex, 'C'):
                EnumLoc = Proc6(Ident1)
            CharIndex = chr(ord(CharIndex) + 1)
        IntLoc3 = IntLoc2 * IntLoc1
        IntLoc2 = IntLoc3 // IntLoc1
        IntLoc2 = 7 * (IntLoc3 - IntLoc2) - IntLoc1
        IntLoc1 = Proc2(IntLoc1)
    return time.ticks_diff(time.ticks_ms(), t0)

def Proc1(PtrParIn):
    PtrParIn[0] = NextRecord = list(PtrGlb)
    PtrParIn[2] = 5
    NextRecord[2] = PtrParIn[2]
    NextRecord[0] = PtrParIn[0]
    NextRecord[0] = Proc3(NextRecord[0])
    if NextRecord[1] == Ident1:
        NextRecord[2] = 6
        NextRecord[1] = Proc6(PtrParIn[1])
        NextRecord[0] = PtrGlb[0]
        NextRecord[2] = Proc7(NextRecord[2], 10)
    else:
        PtrParIn = list(NextRecord)
    NextRecord[0] = None
    return PtrParIn

def Proc2(IntParIO):
    IntLoc = IntParIO + 10
    while 1:
        if Char1Glob == 'A':
            IntLoc = IntLoc - 1
            IntParIO = IntLoc - IntGlob
            EnumLoc = Ident1
        if EnumLoc == Ident1:
            break
    return IntParIO

def Proc3(PtrParOut):
    global IntGlob
    if PtrGlb is not None:
        PtrParOut = PtrGlb[0]
    PtrGlb[2] = Proc7(10, IntGlob)
    return PtrParOut

def Proc4():
    global Char2Glob
    BoolLoc = Char1Glob == 'A'
    BoolLoc = BoolLoc or BoolGlob
    Char2Glob = 'B'

def Proc5():
    global Char1Glob, BoolGlob
    Char1Glob = 'A'
    BoolGlob = 0

def Proc6(EnumParIn):
    EnumParOut = EnumParIn
    if not Func3(EnumParIn):
        EnumParOut = Ident4
    if EnumParIn == Ident1:
        EnumParOut = Ident1
    elif EnumParIn == Ident2:
        if IntGlob > 100:
            EnumParOut = Ident1
        else:
            EnumParOut = Ident4
    elif EnumParIn == Ident3:
        EnumParOut = Ident2
    elif EnumParIn == Ident4:
        pass
    elif EnumParIn == Ident5:
        EnumParOut = Ident3
    return EnumParOut

def Proc7(IntParI1, IntParI2):
    IntLoc = IntParI1 + 2
    return IntParI2 + IntLoc

def Proc8(Array1Par, Array2Par, IntParI1, IntParI2):
    global IntGlob
    IntLoc = IntParI1 + 5
    Array1Par[IntLoc] = IntParI2
    Array1Par[IntLoc + 1] = Array1Par[IntLoc]
    Array1Par[IntLoc + 30] = IntLoc
    for IntIndex in range(IntLoc, IntLoc + 2):
        Array2Par[IntLoc][IntIndex] = IntLoc
    Array2Par[IntLoc][IntLoc - 1] = Array2Par[IntLoc][IntLoc - 1] + 1
    Array2Par[IntLoc + 20][IntLoc] = Array1Par[IntLoc]
    IntGlob = 5

def Func1(CharPar1, CharPar2):
    CharLoc1 = CharPar1
    CharLoc2 = CharLoc1
    if CharLoc2 != CharPar2:
        return Ident1
    return Ident2

def Func2(StrParI1, StrParI2):
    IntLoc = 1
    while IntLoc <= 1:
        if Func1(StrParI1[IntLoc], StrParI2[IntLoc + 1]) == Ident1:
            CharLoc = 'A'
            IntLoc = IntLoc + 1
    if CharLoc >= 'W' and CharLoc <= 'Z':
        IntLoc = 7
    if CharLoc == 'X':
        return 1
    if StrParI1 > StrParI2:
        IntLoc = IntLoc + 7
        return 1
    return 0

def Func3(EnumParIn):
    return EnumParIn == Ident3

for n in range(3):
    ms = Proc0(LOOPS)
    print("pystone run", n + 1, "loops", LOOPS, "ms", ms, "pystones/s", LOOPS * 1000 // ms)
