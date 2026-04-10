// SNDH file exporter for ft2-clone Atari mode
// See ft2_sndh_export.h for format documentation.

#ifdef _MSC_VER
#pragma warning(disable: 4996)
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>

#include "ft2_sndh_export.h"
#include "ft2_atari_replayer.h"
#include "ft2_replayer.h"
#include "ft2_header.h"

/*
 * Pre-assembled Motorola 68000 SNDH replayer for register-dump playback.
 *
 * The code layout (word-aligned):
 *
 * Offset  0: sndh_init  — called once; stores song_frames address in frame_ptr
 * Offset  4: sndh_play  — called each VBL; writes R0-R13 to YM2149
 * Offset 30: sndh_stop  — minimal stop: silence all channels, rts
 * Offset 38: frame_ptr  — dc.l 0  (pointer into frame data)
 * Offset 42: song_frames — frame data appended here
 *
 * Assembly (68000, Atari ST memory map, YM2149 at $FFFF8800):
 *
 * sndh_init:
 *   lea   song_frames(pc), a0   ; 4E BA xxxx
 *   move.l a0, frame_ptr(pc)   ; 21C8 xxxx (move.l a0, (abs).w) — wrong;
 *                                ; use: lea frame_ptr(pc),a1 / move.l a0,(a1)
 *   rts                         ; 4E 75
 *
 * We use the following encoding:
 *   lea song_frames(pc),a0   48 7A 00 28    (pc+2 + 0x28 = offset 42)
 *   lea frame_ptr(pc),a1     49 7A 00 22    (pc+2 + 0x22 = offset 38)
 *   move.l a0,(a1)           22 88
 *   rts                      4E 75
 *
 * sndh_play (offset 8):
 *   movea.l frame_ptr(pc),a0     20 7A 00 1C    (pc+2+0x1C = offset 38)
 *   tst.b  (a0)                  4A 10
 *   beq.s  .done                 67 12
 *   lea    $FFFF8800.w,a1        43 F9 FF FF 88 00
 *   moveq  #13,d1                72 0D
 * .loop:
 *   move.b d1,(a1)               12 81
 *   move.b (a0)+,2(a1)           13 58 00 02
 *   dbf    d1,.loop              51 C9 FF F8
 *   lea    frame_ptr(pc),a1      49 7A 00 0E    (pc+2+0x0E = offset 38)
 *   move.l a0,(a1)               22 88
 * .done:
 *   rts                          4E 75
 *
 * sndh_stop (offset 0x28 = 40):
 *   rts    4E 75  (+ pad)
 *
 * frame_ptr (offset 0x2A = 42): dc.l 0
 * song_frames (offset 0x2E = 46): frame data starts here
 *
 * Let's recalculate byte offsets carefully, with the full byte sequence:
 *
 * Byte offsets from start of sndh_replayer_68k[]:
 *
 *  0: sndh_init
 *     48 7A 00 28   lea  song_frames(pc),a0   ; pc=0, disp=0x28 → target=0+2+0x28=0x2A+4=0x2E ✓
 *     49 7A 00 22   lea  frame_ptr(pc),a1      ; pc=4, disp=0x22 → target=4+2+0x22=0x28... hmm
 *
 * Let me redo this from scratch with exact byte offsets.
 *
 * Layout:
 *  [0x00]  sndh_init  (10 bytes)
 *  [0x0A]  sndh_play  (32 bytes)
 *  [0x2A]  sndh_stop  (2 bytes + 2 pad)
 *  [0x2E]  frame_ptr  (4 bytes)  ← pointer updated at runtime
 *  [0x32]  song_frames ← data appended here at export time
 *
 * sndh_init at 0x00:
 *   lea  song_frames(pc),a0    ; 48 7A 00 30   pc=0x00+2=0x02, disp=0x30 → 0x02+0x30=0x32 ✓
 *   lea  frame_ptr(pc),a1      ; 49 7A 00 2A   pc=0x04+2=0x06, disp=0x2A → 0x06+0x28=0x2E ✓
 *                              ; wait: 49 7A is lea d16(pc),An? Check:
 *                              ; lea (d16,PC),An = 41FA xxxx, reg An=1 → 43FA → no
 *                              ; Actually: lea (d16,PC),An encoding:
 *                              ;   opcode = 0100 An 111 010 PC = 0100 001 111 111 010
 *                              ;   An=0: 0100 000 111 111 010 = 41FA
 *                              ;   An=1: 0100 001 111 111 010 = 43FA
 *   lea  song_frames(pc),a0    ; 41 FA 00 30
 *   lea  frame_ptr(pc),a1      ; 43 FA 00 28   pc=0x06, disp=0x28 → 0x06+0x28=0x2E ✓
 *   move.l a0,(a1)             ; 22 88          (move.l a0,(a1))... wait that's move.l a0,d0
 *                              ; move.l a0,(a1) = MOVE.L A0,(A1)
 *                              ; = 0010 dst_ea src_ea
 *                              ; dst=(A1) = mode 010, reg 001 = 010 001
 *                              ; src=A0 = mode 001, reg 000 = 001 000
 *                              ; opword = 0010 010 001 001 000 = 0010 0100 0100 1000 = 24 48 -- CHECK
 *                              ; Actually MOVE.L = size 10, so: 0010 size=10 dst src
 *                              ; MOVE.L (A1),(A0): no, MOVE.L A0,(A1) means:
 *                              ; EA_dst=(A1)=mode 010,reg 001; EA_src=A0=mode 001,reg 000
 *                              ; opword = 00 10 (dst.mode)(dst.reg) (src.mode)(src.reg)
 *                              ;        = 00 10 010 001 001 000
 *                              ;        = 0010 0100 0100 1000 = 0x2448
 *                              ; BUT: this is MOVE.L source,dest, where:
 *                              ;   MOVE <ea>,<ea> = 00 ss (dst Dn/mode) (src mode/reg)
 *                              ;   size ss=10 for .L
 *                              ;   dest = (A1), mode=010, reg=001
 *                              ;   src  = A0,   mode=001, reg=000
 *                              ;   word = 0010 010001 001000 = 0x2448
 *   rts                        ; 4E 75
 *
 * That gives sndh_init = 10 bytes: 41 FA 00 30 43 FA 00 28 24 48 4E 75 -- wait that's 12.
 * Let me recount: 41 FA + 00 30 = 4 bytes (lea a0)
 *                 43 FA + 00 28 = 4 bytes (lea a1)  → pc is now at 0x08
 *                 24 48         = 2 bytes (move.l a0,(a1))
 *                 4E 75         = 2 bytes (rts)
 * sndh_init = 12 bytes.  Ends at 0x0C.
 *
 * sndh_play at 0x0C:
 *   movea.l  frame_ptr(pc),a0   ; 20 7A disp
 *                               ; MOVEA.L (d16,PC),A0 = MOVEA.L with An=A0
 *                               ; = 0010 000 001 111 010 = 0x207A
 *                               ; pc=0x0C+2=0x0E, disp = 0x2E - 0x0E = 0x20 → disp=0x0020? Hmm wait
 *                               ; frame_ptr is at 0x2E, pc after reading opword = 0x0E
 *                               ; disp = 0x2E - 0x0E = 0x20  → 20 7A 00 20   (4 bytes, pc=0x10)
 *   tst.b  (a0)                 ; 4A 10                       (2 bytes, pc=0x12)
 *   beq.s  .done                ; 67 xx  (we'll compute offset after)  (2 bytes, pc=0x14)
 *   lea    $FFFF8800.w,a1       ; 43 F8 88 00    wait: LEA (abs.w),An
 *                               ; LEA = 0100 An 111 <ea>
 *                               ; An=1: 0100 001 111 xxx
 *                               ; ea = (xxx).W = mode 111 reg 000 = 111000
 *                               ; opword = 0100 001 111 111 000 = 0x43F8
 *                               ; then 16-bit abs = 0x8800 (sign-extended = 0xFFFF8800)
 *                               ; 43 F8 88 00  (4 bytes, pc=0x18)
 *   moveq  #13,d1               ; 72 0D         (2 bytes, pc=0x1A)
 * .loop:  (at 0x1A)
 *   move.b d1,(a1)              ; 12 81         (2 bytes, pc=0x1C)
 *                               ; MOVE.B D1,(A1): size B=01, dst=(A1)=010 001, src=D1=000 001
 *                               ; = 00 01 010001 000001 = 0x1241? No...
 *                               ; MOVE <ea>,<ea>: 00 ss Dn Rn <ea>
 *                               ; Actually: MOVE.B D1,(A1):
 *                               ; = 0001 (dst.mode)(dst.reg) (src.mode)(src.reg)
 *                               ; dst = (A1) = 010, 001
 *                               ; src = D1 = 000, 001
 *                               ; = 0001 010 001 000 001 = 0x1241
 *                               ; Hmm, but the problem says 12 81. Let me check:
 *                               ; 0x12 = 0001 0010, 0x81 = 1000 0001
 *                               ; That's 0x1281 = 0001 0010 1000 0001
 *                               ; = 0001 (01)(001) (000 001) -- wait: 00 01 01 001 000 001
 *                               ; dst = 01 001 = (A1), src = 000 001 = D1
 *                               ; But size = 01 = .B. So 0001 dst_mode dst_reg src_mode src_reg
 *                               ; 0001 010 001 000 001 = 0x1241 not 0x1281
 *                               ; I think the problem statement has the assembly right but opcodes wrong
 *                               ; Let me just compute correct opcodes.
 *
 * I'll compute the correct opcodes properly.
 *
 * MOVE.B D1,(A1):
 *   Format: 00 ss (dst.mode<<3|dst.reg) (src.mode<<3|src.reg)
 *   ss=01 (byte), dst=(A1)→mode=010,reg=001 → 010001=0x11, src=D1→mode=000,reg=001→000001=0x01
 *   opword = 0001 (010)(001) (000)(001) = 0001 010 001 000 001 = 0x1241
 *
 * MOVE.B (A0)+, 2(A1):
 *   src=(A0)+→mode=011,reg=000, dst=d16(A1)→mode=101,reg=001
 *   opword = 0001 (101)(001) (011)(000) = 0001 101 001 011 000 = 0x1A58
 *   then d16 = 0x0002
 *   → 1A 58 00 02  (4 bytes)
 *
 * DBF D1,.loop:
 *   = 0101 001 011 001 001 = 0x51C9
 *   disp = .loop - (pc after opword) = 0x1A - (0x1E+2) = 0x1A - 0x20 = -6 = 0xFFFA
 *   → 51 C9 FF FA  (4 bytes)
 *
 * After .loop, pc = 0x22:
 *   lea  frame_ptr(pc),a1  ; 43 FA disp
 *                          ; pc = 0x24, disp = 0x2E - 0x24 = 0x0A
 *                          ; 43 FA 00 0A  (4 bytes, pc=0x26)
 *   move.l a0,(a1)         ; 24 48  (2 bytes, pc=0x28) -- WRONG opcode above
 *                          ; MOVE.L A0,(A1):
 *                          ; ss=10, dst=(A1)=010 001, src=A0=001 000
 *                          ; = 0010 010 001 001 000 = 0x2448
 *                          ; → 24 48  (2 bytes, pc=0x2A)
 * .done: (at 0x2A)
 *   rts                    ; 4E 75  (2 bytes)
 *
 * sndh_play ends at 0x2C. Total play routine bytes: 0x2C - 0x0C = 0x20 = 32 bytes.
 *
 * Now the beq.s displacement: .done is at 0x2A, beq.s is at 0x14
 * After opword pc=0x16, disp = 0x2A - 0x16 = 0x14 = 20
 * → beq.s = 67 14
 *
 * sndh_stop at 0x2C:
 *   rts = 4E 75 (2 bytes)
 *   (word pad: 00 00)
 *
 * frame_ptr at 0x30:
 *   dc.l 0 = 00 00 00 00 (4 bytes)
 *
 * song_frames at 0x34: data follows
 *
 * Now re-verify sndh_init displacements (sndh_init at 0x00):
 *   lea song_frames(pc),a0: pc=0x02, target=0x34, disp=0x34-0x02=0x32 → 41 FA 00 32
 *   lea frame_ptr(pc),a1:  pc=0x06, target=0x30, disp=0x30-0x06=0x2A → 43 FA 00 2A
 *   move.l a0,(a1): 24 48
 *   rts: 4E 75
 *   = 12 bytes [0x00..0x0B]
 *
 * sndh_play at 0x0C:
 *   movea.l frame_ptr(pc),a0: pc=0x0E, target=0x30, disp=0x30-0x0E=0x22 → 20 7A 00 22
 *   tst.b (a0): 4A 10
 *   beq.s .done: 67 14  (disp=20, .done at 0x2A, pc after beq.s=0x16, 0x2A-0x16=0x14=20) ✓
 *   lea $FFFF8800.w,a1: 43 F8 88 00
 *   moveq #13,d1: 72 0D
 *   .loop at 0x1A:
 *   move.b d1,(a1): 12 41
 *   move.b (a0)+,2(a1): 1A 58 00 02
 *   dbf d1,.loop: 51 C9 FF FA  (disp=-6, 0x1A-0x20=-6=0xFFFA) ✓
 *   lea frame_ptr(pc),a1: pc=0x26, target=0x30, disp=0x30-0x26=0x0A → 43 FA 00 0A
 *   move.l a0,(a1): 24 48
 *   .done at 0x2A:
 *   rts: 4E 75
 *   = 0x2C - 0x0C = 32 bytes ✓
 *
 * sndh_stop at 0x2C: 4E 75
 * pad: 00 00
 * frame_ptr at 0x30: 00 00 00 00
 * song_frames at 0x34: data starts here
 *
 * SNDH init vector (from start of binary):   0x00  (sndh_init)
 * SNDH play vector (from start of binary):   0x0C  (sndh_play)
 * SNDH stop vector (from start of binary):   0x2C  (sndh_stop)
 *
 * The SNDH file structure:
 *   "SNDH"                 (magic, at absolute 0 in the binary ← part of header!)
 * Wait — SNDH format: the entire file starts with the ASCII header.
 * The header IS the beginning of the file; it ends with "HDNS".
 * The 68000 code immediately follows "HDNS".
 *
 * The SNDH sub-song init/play/stop vectors in the header point to ABSOLUTE
 * addresses IN THE LOADED BINARY. When loaded at $xxx on Atari, the vectors
 * are at PC-relative offsets from the start of the loaded binary.
 * Actually SNDH doesn't embed explicit init/play/stop addresses in the header
 * for single-subsong files — they are the first 3 longwords (jump vectors):
 *   +0x00: JMP init
 *   +0x04: JMP play
 *   +0x08: JMP stop
 * These are absolute JMP instructions if using 68000 JMP, OR the replayer
 * is called via JSR from a standard wrapper at addresses 0, 4, 8 from the
 * code start. The most common practice: put actual code at those entry points.
 *
 * Simpler approach: put RTS at 0, 4, 8 and separate init/play/stop routines
 * so that the player can find them as indicated by the SNDH header tags.
 *
 * Actually the standard SNDH calling convention is:
 *  - The file is loaded at some address X.
 *  - Player calls X+0 for init (with subtune in d0)
 *  - Player calls X+4 for play (each VBL)
 *  - Player calls X+8 for exit
 * X+0, X+4, X+8 are the fixed entry points; they can be JMP instructions or
 * actual code. Our replayer puts init/play/stop directly at those offsets.
 *
 * Final layout:
 *  [X+0x00]  sndh_init  (12 bytes) → 0x00..0x0B
 *  [X+0x0C]  sndh_play  (32 bytes) → 0x0C..0x2B
 *  [X+0x2C]  sndh_stop  (4 bytes)  → 0x2C..0x2F
 *  [X+0x30]  frame_ptr  (4 bytes)  → 0x30..0x33
 *  [X+0x34]  song_frames → data
 *
 * The SNDH "!#" initial subtune and "##" subtune count don't affect entry points.
 * However the standard requires init at offset 0, play at offset 4, stop at 8.
 * Our init is 12 bytes which won't fit a JMP at offset 4. Let's restructure:
 *
 * Use three JMP instructions at 0, 4, 8 that jump to the actual routines:
 *  [0x00] JMP abs.l init_actual  → 4E F9 00 00 00 XX  (6 bytes each)
 *  [0x06] JMP abs.l play_actual  → (but then play at offset 4 is in the middle of JMP1)
 *
 * That won't work. The simplest approach: make each routine exactly 4 bytes or
 * put them at the exact offsets. Standard SNDH players tolerate any init length
 * as long as the code at offset 0 returns correctly (RTS from init, RTS from play).
 *
 * Actually re-reading SNDH spec: init is at offset 0, play is at 4, stop is 8.
 * So:
 *  [0x00] init code (must be exactly 4 bytes or end with RTS before offset 4)
 *   → This is impossible for our 12-byte init. We need JMPs.
 *
 * Use JMP table at start:
 *  [0x00] JMP init_code      ; 4E F9 00 00 00 NN  (6 bytes, but offset 4 conflicts)
 *
 * Or: BRA.W to routines:
 *  [0x00] BRA.W init_code    ; 60 00 00 xx (4 bytes)
 *  [0x04] BRA.W play_code    ; 60 00 00 xx (4 bytes)
 *  [0x08] BRA.W stop_code    ; 60 00 00 xx (4 bytes)
 *
 * Layout with BRA table:
 *  [0x00]  BRA.W init_code   ; 60 00 + 2-byte disp
 *  [0x04]  BRA.W play_code
 *  [0x08]  BRA.W stop_code
 *  [0x0C]  init_code (12 bytes)  → 0x0C..0x17
 *  [0x18]  play_code (32 bytes)  → 0x18..0x37
 *  [0x38]  stop_code (2 bytes)   → 0x38..0x39
 *  [0x3A]  pad (2 bytes)         → 0x3A..0x3B
 *  [0x3C]  frame_ptr (4 bytes)   → 0x3C..0x3F
 *  [0x40]  song_frames           → data starts here
 *
 * BRA.W disp is relative to PC after BRA opword (pc = current_pos + 4 for BRA.W):
 *  BRA.W init_code at 0x00: pc_after=0x04, target=0x0C, disp=0x0C-0x04=0x08 → 60 00 00 08
 *  BRA.W play_code at 0x04: pc_after=0x08, target=0x18, disp=0x18-0x08=0x10 → 60 00 00 10
 *  BRA.W stop_code at 0x08: pc_after=0x0C, target=0x38, disp=0x38-0x0C=0x2C → 60 00 00 2C
 *
 * init_code at 0x0C:
 *   lea song_frames(pc),a0: pc=0x0E, target=0x40, disp=0x40-0x0E=0x32 → 41 FA 00 32
 *   lea frame_ptr(pc),a1:  pc=0x12, target=0x3C, disp=0x3C-0x12=0x2A → 43 FA 00 2A
 *   move.l a0,(a1): 24 48
 *   rts: 4E 75
 *   = 12 bytes, ends at 0x18 ✓
 *
 * play_code at 0x18:
 *   movea.l frame_ptr(pc),a0: pc=0x1A, target=0x3C, disp=0x3C-0x1A=0x22 → 20 7A 00 22
 *   tst.b (a0): 4A 10
 *   beq.s .done: 67 14 (disp=20; .done at 0x36, pc_after_beq=0x1E+2=0x20, 0x36-0x20=0x16=22)
 *   Hmm let me recompute:
 *   play_code:
 *   [0x18] movea.l fp(pc),a0  → 20 7A 00 22  (4 bytes → 0x1C)
 *   [0x1C] tst.b (a0)          → 4A 10        (2 bytes → 0x1E)
 *   [0x1E] beq.s .done         → 67 xx        (2 bytes → 0x20)
 *   [0x20] lea $FFFF8800.w,a1  → 43 F8 88 00  (4 bytes → 0x24)
 *   [0x24] moveq #13,d1        → 72 0D        (2 bytes → 0x26)
 *   .loop at 0x26:
 *   [0x26] move.b d1,(a1)      → 12 41        (2 bytes → 0x28)
 *   [0x28] move.b (a0)+,2(a1)  → 1A 58 00 02  (4 bytes → 0x2C)
 *   [0x2C] dbf d1,.loop        → 51 C9 FF F8  (4 bytes → 0x30) disp=0x26-0x2E=−8=0xFFF8 ✓
 *   [0x30] lea frame_ptr(pc),a1→ 43 FA 00 0A  pc=0x32, target=0x3C, disp=0x3C-0x32=0x0A ✓ (4b→0x34)
 *   [0x34] move.l a0,(a1)      → 24 48        (2 bytes → 0x36)
 *   .done at 0x36:
 *   [0x36] rts                 → 4E 75        (2 bytes → 0x38)
 *   = 0x38-0x18 = 32 bytes ✓
 *
 * beq.s disp: target=0x36, pc_after_beq.s=0x20, disp=0x36-0x20=0x16=22 → 67 16
 *
 * stop_code at 0x38:
 *   [0x38] rts → 4E 75  (2 bytes → 0x3A)
 *   [0x3A] pad → 00 00  (2 bytes → 0x3C)
 *
 * frame_ptr at 0x3C:
 *   [0x3C] dc.l 0 → 00 00 00 00 (4 bytes → 0x40)
 *
 * song_frames at 0x40: data starts here
 *
 * SNDH entry points in binary (from start of code, AFTER header):
 *   init: offset 0x00 (BRA.W to actual init at 0x0C)
 *   play: offset 0x04
 *   stop: offset 0x08
 */
static const uint8_t sndh_replayer_68k[] = {
	/* 0x00 */ 0x60, 0x00, 0x00, 0x08, // BRA.W init_code  (+0x08 → 0x0C)
	/* 0x04 */ 0x60, 0x00, 0x00, 0x10, // BRA.W play_code  (+0x10 → 0x18)
	/* 0x08 */ 0x60, 0x00, 0x00, 0x2C, // BRA.W stop_code  (+0x2C → 0x38)

	/* 0x0C init_code: */
	0x41, 0xFA, 0x00, 0x32, // lea song_frames(pc),a0  (pc=0x0E disp=0x32→0x40)
	0x43, 0xFA, 0x00, 0x2A, // lea frame_ptr(pc),a1    (pc=0x12 disp=0x2A→0x3C)
	0x24, 0x48,             // move.l a0,(a1)
	0x4E, 0x75,             // rts

	/* 0x18 play_code: */
	0x20, 0x7A, 0x00, 0x22, // movea.l frame_ptr(pc),a0 (pc=0x1A disp=0x22→0x3C)
	0x4A, 0x10,             // tst.b (a0)
	0x67, 0x16,             // beq.s .done (→0x36)
	0x43, 0xF8, 0x88, 0x00, // lea $FFFF8800.w,a1
	0x72, 0x0D,             // moveq #13,d1
	/* 0x26 .loop: */
	0x12, 0x41,             // move.b d1,(a1)
	0x1A, 0x58, 0x00, 0x02, // move.b (a0)+,2(a1)
	0x51, 0xC9, 0xFF, 0xF8, // dbf d1,.loop (disp=-8→0x26)
	0x43, 0xFA, 0x00, 0x0A, // lea frame_ptr(pc),a1 (pc=0x32 disp=0x0A→0x3C)
	0x24, 0x48,             // move.l a0,(a1)
	/* 0x36 .done: */
	0x4E, 0x75,             // rts

	/* 0x38 stop_code: */
	0x4E, 0x75,             // rts
	0x00, 0x00,             // word pad

	/* 0x3C frame_ptr: */
	0x00, 0x00, 0x00, 0x00, // dc.l 0  (filled at runtime)

	/* 0x40 song_frames: frame data appended at export time */
};

/* Size of the fixed header (replayer code + frame_ptr), before frame data */
#define SNDH_REPLAYER_HDR_SIZE  0x40

#define SNDH_MAX_FRAMES     200000
#define SNDH_YM_REGS        14   /* R0..R13 */
#define SNDH_FRAME_SIZE     14   /* one frame = 14 register bytes */
#define SNDH_SENTINEL       0xFF /* end-of-frames sentinel byte */

bool sndhExportToFile(const char *path, const sndhExportSettings_t *settings)
{
	if (path == NULL || settings == NULL)
		return false;

	const uint16_t timerHz = (settings->timerHz > 0) ? settings->timerHz : 50;

	/* --- Render register frames (same approach as YM export) --- */

	uint8_t (*frames)[SNDH_FRAME_SIZE] =
		(uint8_t (*)[SNDH_FRAME_SIZE])calloc(SNDH_MAX_FRAMES, sizeof(*frames));
	if (frames == NULL)
		return false;

	/* Save song state */
	int16_t  savedSongPos     = song.songPos;
	int16_t  savedRow         = song.row;
	int16_t  savedPattNum     = song.pattNum;
	uint16_t savedTick        = song.tick;
	uint16_t savedSpeed       = song.speed;
	uint16_t savedBPM         = song.BPM;
	bool     savedPlaying     = songPlaying;
	int8_t   savedPlayMode    = playMode;
	uint8_t  savedPBreakPos   = song.pBreakPos;
	bool     savedPBreakFlag  = song.pBreakFlag;
	bool     savedPosJumpFlag = song.posJumpFlag;
	int16_t  savedCurrNumRows = song.currNumRows;

	song.songPos     = (int16_t)settings->startPos;
	song.row         = 0;
	song.pattNum     = song.orders[song.songPos];
	song.currNumRows = patternNumRows[song.pattNum];
	if (song.currNumRows <= 0) song.currNumRows = 1;
	song.tick        = 1;
	if (song.speed == 0) song.speed = 6;
	song.pBreakFlag  = false;
	song.posJumpFlag = false;
	song.pBreakPos   = 0;
	songPlaying      = true;
	playMode         = PLAYMODE_SONG;

	atariReplayer_t ar;
	atariReplayer_init(&ar);

	uint32_t numFrames = 0;
	bool     songAlive = true;
	uint32_t fracAcc   = 0;

	while (songAlive && numFrames < SNDH_MAX_FRAMES)
	{
		fracAcc += (uint32_t)song.BPM * song.speed * 2;
		uint32_t divisor = (uint32_t)5 * timerHz;
		uint32_t ticksThisFrame = fracAcc / divisor;
		fracAcc %= divisor;
		if (ticksThisFrame == 0)
			ticksThisFrame = 1;

		for (uint32_t t = 0; t < ticksThisFrame && songAlive; t++)
		{
			songAlive = atariReplayer_tick(&ar);
			if (song.songPos > (int16_t)settings->stopPos)
			{
				songAlive = false;
				break;
			}
		}

		atariReplayer_getRegs(&ar, frames[numFrames]);
		numFrames++;
	}

	/* Restore song state */
	song.songPos     = savedSongPos;
	song.row         = savedRow;
	song.pattNum     = savedPattNum;
	song.tick        = savedTick;
	song.speed       = savedSpeed;
	song.BPM         = savedBPM;
	songPlaying      = savedPlaying;
	playMode         = savedPlayMode;
	song.pBreakPos   = savedPBreakPos;
	song.pBreakFlag  = savedPBreakFlag;
	song.posJumpFlag = savedPosJumpFlag;
	song.currNumRows = savedCurrNumRows;

	/* --- Write SNDH file --- */

	FILE *f = fopen(path, "wb");
	if (f == NULL)
	{
		free(frames);
		return false;
	}

	/* SNDH magic */
	fwrite("SNDH", 1, 4, f);

	/* Metadata tags */
	if (settings->title[0] != '\0')
	{
		fwrite("TITL", 1, 4, f);
		fwrite(settings->title, 1, strlen(settings->title) + 1, f);
	}
	if (settings->author[0] != '\0')
	{
		fwrite("COMM", 1, 4, f);
		fwrite(settings->author, 1, strlen(settings->author) + 1, f);
	}
	if (settings->year[0] != '\0')
	{
		fwrite("YEAR", 1, 4, f);
		fwrite(settings->year, 1, strlen(settings->year) + 1, f);
	}

	/* Timer-C tag: "TC50" or "TC60" */
	char tcTag[6];
	snprintf(tcTag, sizeof(tcTag), "TC%02u", (unsigned)timerHz);
	fwrite(tcTag, 1, 4, f);
	fputc(0, f);

	/* Subtune count and default */
	fwrite("##01", 1, 4, f); fputc(0, f);
	fwrite("!#01", 1, 4, f); fputc(0, f);

	/* HDNS — must be on an even byte boundary.
	   We count bytes written so far after the "SNDH" magic (4 bytes): */
	long hdrBodyLen = ftell(f) - 4; /* bytes after "SNDH" */
	if (hdrBodyLen & 1)
		fputc(0, f); /* pad to even */

	/* HDNS end-of-header marker */
	fwrite("HDNS", 1, 4, f);

	/* --- 68000 replayer code (fixed part) --- */
	/* Copy the template and patch frame_ptr and song_frames fields */
	uint8_t replayerBuf[SNDH_REPLAYER_HDR_SIZE];
	memcpy(replayerBuf, sndh_replayer_68k, SNDH_REPLAYER_HDR_SIZE);

	/* frame_ptr is at offset 0x3C in replayerBuf — reset to 0; runtime fills it */
	replayerBuf[0x3C] = 0;
	replayerBuf[0x3D] = 0;
	replayerBuf[0x3E] = 0;
	replayerBuf[0x3F] = 0;

	fwrite(replayerBuf, 1, SNDH_REPLAYER_HDR_SIZE, f);

	/* --- Frame data: numFrames × 14 bytes, followed by sentinel 0xFF --- */
	for (uint32_t fr = 0; fr < numFrames; fr++)
		fwrite(frames[fr], 1, SNDH_FRAME_SIZE, f);

	fputc(SNDH_SENTINEL, f);

	/* Word-align total file */
	long fileLen = ftell(f);
	if (fileLen & 1)
		fputc(0, f);

	fclose(f);
	free(frames);
	return true;
}
