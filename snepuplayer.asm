.include "/usr/share/avra/m8def.inc"

.def prod_low    = r0
.def prod_high   = r1

; SN79489 registers
.def tone_0      = r2
.def tone_1      = r3
.def tone_2      = r4
.def counter_0   = r5
.def counter_1   = r6
.def counter_2   = r7

.def volume_0    = r16 ; Volume and output registers must >= r16 to use muls
.def volume_1    = r17
.def volume_2    = r18
.def output_0    = r19
.def output_1    = r20
.def output_2    = r21

; Working variables
.def delay_q     = r9
.def output      = r22
.def do_update   = r23
.def tick_count  = r24
.def tick_goal   = r25
.def tick_temp   = r26
.def main_temp   = r27
.def frame_head  = r28

; r30 and r31 are reserved for indexing data in flash (Z-register)

.org 0x0000 ; Reset
    rjmp Init

.org 0x0006 ; Timer1 Compare Match A
    rjmp Tick

.org 0x0012
    reti    ; Catch-all for unexpected interrupts

    ; Rather than ticking at the rate of a real Master System,
    ; simulate the PSG at one quarter its normal speed. This
    ; allows us to use 8-bit timers in place of 10-bit timers.
Tick:
    inc     tick_count

Tone0_decrement:
    tst     counter_0
    breq    Tone0_toggle
    dec     counter_0

Tone0_toggle:
    ; TODO: When tone=0, output=1
    tst     counter_0
    brne    Tone1_decrement
    mov     counter_0,  tone_0
    neg     output_0
    ; ldi     do_update,  0x01

Tone1_decrement:
    tst     counter_1
    breq    Tone1_toggle
    dec     counter_1

Tone1_toggle:
    tst     counter_1
    brne    Tone2_decrement
    mov     counter_1,  tone_1
    neg     output_1
    ; ldi     do_update,  0x01

Tone2_decrement:
    tst     counter_2
    breq    Tone2_toggle
    dec     counter_2

Tone2_toggle:
    tst     counter_2
    brne    Noise_decrement
    mov     counter_2,  tone_2
    neg     output_2
    ; ldi     do_update,  0x01


Noise_decrement:
    ; TODO: Implement the noise channel


Update:
    ; tst     do_update         ; Trying to skip redundant updates caused garbage sound
    ; breq    Tick_done
    ; clr     do_update

    muls    output_0,   volume_0 ; We pre-invert the volume in convert.c
    mov     output,     prod_low
    muls    output_1,   volume_1
    add     output,     prod_low
    muls    output_2,   volume_2
    add     output,     prod_low

    ; Centre signed output as 128 for unsigned output
    ldi     tick_temp,  128
    add     output,     tick_temp
    out     OCR2,       output

Tick_done:
    reti

; 932 interrupts per 1/60
Delay:
    ; Wait for 233 interrupts per quarter-1/60
    mov     tick_goal,  tick_count
    ldi     main_temp,  233
    add     tick_goal,  main_temp

Delay_keep_waiting:
    cp      tick_goal,  tick_count
    brne    Delay_keep_waiting
    dec     delay_q
    brne    Delay
    ret

Init:
    ; Set up the stack pointer
    ldi     main_temp,  low(RAMEND)
    out     spl,        main_temp
    ldi     main_temp,  high(RAMEND)
    out     sph,        main_temp

    ; Timer1: Tick source, CTC mode with no prescaling
    ldi     main_temp,  0x00
    out     TCCR1A,     main_temp
    ldi     main_temp,  0x09
    out     TCCR1B,     main_temp

    ; Timer1: Interrupt on counting to 143
    ldi     main_temp,  0x00
    out     OCR1AH,     main_temp
    ldi     main_temp,  0x8f
    out     OCR1AL,     main_temp
    ldi     main_temp,  0x10
    out     TIMSK,      main_temp

    ; Timer2: Fast-PWM Output on PortB-3
    ldi     main_temp,  0x69
    out     TCCR2,      main_temp
    ldi     main_temp,  0x08
    out     DDRB,       main_temp

    ; Initial values for SN79489
    ldi     output_0,   0x01
    ldi     output_1,   0xff
    ldi     output_2,   0x01
    ldi     volume_0,   0x00
    ldi     volume_1,   0x00
    ldi     volume_2,   0x00

    ; Enable interrupts
    sei

    ; ----- TEST BEEPS -----
    ; A ~440 Hz Tone0
    ldi     main_temp,  0x0c
    mov     volume_0,   main_temp
    ldi     main_temp,  0x40
    mov     tone_0,     main_temp
    ldi     main_temp,  120
    mov     delay_q,    main_temp
    rcall   Delay
    ; C ~523 Hz Tone0
    ldi     main_temp,  0x0c
    mov     volume_0,   main_temp
    ldi     main_temp,  0x35
    mov     tone_0,     main_temp
    ldi     main_temp,  120
    mov     delay_q,    main_temp
    rcall   Delay
    ; Quiet
    ldi     main_temp,  0x00
    mov     volume_0,   main_temp
    ldi     main_temp,  120
    mov     delay_q,    main_temp
    rcall   Delay
    ; ----- TEST BEEPS END -----

Main:
    ; Set up our pointer to the start of the music data
    ldi     ZH,         high (Music_data << 1)
    ldi     ZL,         low  (Music_data << 1)

Play_frame:

    lpm     frame_head, Z+

    ; Header bits:
    ;  0 - Tone0 data follows
    ;  1 - Tone1 data follows
    ;  2 - Tone2 data follows
    ;  3 - Noise data follows
    ;  4 - Volume0/1 data follows
    ;  5 - Volume2/N data follows
    ;  6 - LSB of 1/60 delays
    ;  7 - MSB of 1/60 delays

    ; Restart when we hit a zero byte.
    ; Perhaps add some delay when we do this.
    tst     frame_head,
    breq    Main

    ; Update tone registers
    sbrc    frame_head, 0
    lpm     tone_0,     Z+
    sbrc    frame_head, 1
    lpm     tone_1,     Z+
    sbrc    frame_head, 2
    lpm     tone_2,     Z+

    ; Update volume registers
    sbrs    frame_head, 4
    rjmp    Skip_vol_0_1
    lpm     main_temp,  Z+
    mov     volume_0,   main_temp ; Hopefully there is no tick before the next line
    andi    volume_0,   0x0f
    lsr     main_temp
    lsr     main_temp
    lsr     main_temp
    lsr     main_temp
    mov     volume_1,   main_temp
Skip_vol_0_1:

    sbrs    frame_head, 5
    rjmp    Skip_vol_2_n
    lpm     main_temp,  Z+
    andi    main_temp,  0x0f
    mov     volume_2,   main_temp
Skip_vol_2_n:

    ; Delay (in quarter-frames)
    andi    frame_head, 0xc0
    lsr     frame_head
    lsr     frame_head
    lsr     frame_head
    lsr     frame_head
    mov     delay_q, frame_head
    rcall   Delay

    ; Loop
    rjmp    Play_frame

Music_data:
#include "./music_data.inc"
