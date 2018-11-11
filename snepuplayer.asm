.include "/usr/share/avra/m8def.inc"

.def prod_low    = r0
.def prod_high   = r1

; SN79489 registers (low)
.def tone_0      = r2
.def tone_1      = r3
.def tone_2      = r4
.def noise       = r5
.def counter_0   = r6
.def counter_1   = r7
.def counter_2   = r8
.def counter_3   = r9
.def output_3    = r10
.def lfsr_low    = r11
.def lfsr_high   = r12

; SN79489 registers (high)
; High registers are used for volume and output to allow multiplication.
.def volume_0    = r16
.def volume_1    = r17
.def volume_2    = r18
.def volume_3    = r19
.def output_0    = r20
.def output_1    = r21
.def output_2    = r22
.def output_lfsr = r23

; Working variables (low)
.def delay_q     = r13
.def output      = r14

; Working variables (high)
.def tick_count  = r25
.def tick_goal   = r26
.def tick_temp   = r27
.def main_temp   = r28
.def frame_head  = r29

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
    breq    Tone0_skip_decrement
    dec     counter_0
Tone0_skip_decrement:

    ; TODO: When tone=0, output=1
    tst     counter_0
    brne    Tone1_decrement
    mov     counter_0,  tone_0
    neg     output_0

Tone1_decrement:
    tst     counter_1
    breq    Tone1_skip_decrement
    dec     counter_1
Tone1_skip_decrement:

    tst     counter_1
    brne    Tone2_decrement
    mov     counter_1,  tone_1
    neg     output_1

Tone2_decrement:
    tst     counter_2
    breq    Tone2_skip_decrement
    dec     counter_2
Tone2_skip_decrement:

    tst     counter_2
    brne    Noise_decrement
    mov     counter_2,  tone_2
    neg     output_2

Noise_decrement:
    tst     counter_3
    breq    Noise_skip_decrement
    dec     counter_3
Noise_skip_decrement:

    tst     counter_3
    brne    Noise_shift_done

    ; The reset value of counter_3 depends on the lower two bits of
    ; the noise register value. We'll also need to divide these reset
    ; values to compensate for simulating 1/4 the clock speed.
    ;  0 - Reset to 0x04 (0x10)
    ;  1 - Reset to 0x08 (0x20)
    ;  2 - Reset to 0x10 (0x40)
    ;  3 - Reset to Tone2
    mov     tick_temp,  noise
    andi    tick_temp,  0x03
    cpi     tick_temp,  0x00
    breq    Noise_reset_10
    cpi     tick_temp,  0x01
    breq    Noise_reset_20
    cpi     tick_temp,  0x02
    breq    Noise_reset_40
    cpi     tick_temp,  0x03
    breq    Noise_reset_T2
Noise_reset_10:
    ldi     tick_temp,  0x04 ; 0x10
    mov     counter_3,  tick_temp
    rjmp    Noise_reset_done
Noise_reset_20:
    ldi     tick_temp,  0x08 ; 0x20
    mov     counter_3,  tick_temp
    rjmp    Noise_reset_done
Noise_reset_40:
    ldi     tick_temp,  0x10 ; 0x40
    mov     counter_3,  tick_temp
    rjmp    Noise_reset_done
Noise_reset_T2:
    mov     counter_3,  tone_2
Noise_reset_done:

    ; Invert the output and check for a -1 -> +1 transition
    neg     output_3
    mov     tick_temp,  output_3
    cpi     tick_temp,  0x01
    brne    Noise_shift_done

    ; Output is the least-significant bit of the lfsr
    mov     output_lfsr, lfsr_low
    andi    output_lfsr, 0x01

    ; Perform the shift
    ldi     tick_temp,  0x80
    lsr     lfsr_low
    sbrc    lfsr_high,  0
    or      lfsr_low,   tick_temp
    lsr     lfsr_high

    ; Feed bit 0 back into the lfsr
    sbrc    output_lfsr, 0
    or      lfsr_high,  tick_temp

    ; For periodic noise, we are now done. (only bit 0 tapped)
    sbrs    noise,      2
    rjmp    Noise_shift_done

    ; For white noise, we also want to tap what was bit 3
    clr     tick_temp
    sbrc    lfsr_low, 2
    ldi     tick_temp,  0x80
    eor     lfsr_high,  tick_temp

Noise_shift_done:

Update_pwm:
    ; Volumes are pre-inverted in convert.c
    muls    output_0,   volume_0
    mov     output,     prod_low
    muls    output_1,   volume_1
    add     output,     prod_low
    muls    output_2,   volume_2
    add     output,     prod_low
    muls    output_lfsr, volume_3
    add     output,     prod_low

    ; Output using 128 as the zero level
    lsl     output
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
    mov     output_3,   output_1
    ldi     volume_0,   0x00
    ldi     volume_1,   0x00
    ldi     volume_2,   0x00
    ldi     volume_3,   0x00

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

    ; Update the noise register and reset the lfsr
    sbrs    frame_head, 3
    rjmp    Skip_noise
    lpm     noise,      Z+
    clr     lfsr_high
    clr     lfsr_low
    inc     lfsr_low
Skip_noise:

    ; Update volume_0 / volume_1 registers
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

    ; Update volume_2 / volume_3 registers
    sbrs    frame_head, 5
    rjmp    Skip_vol_2_n
    lpm     main_temp,  Z+
    mov     volume_2,   main_temp
    andi    volume_2,   0x0f
    lsr     main_temp
    lsr     main_temp
    lsr     main_temp
    lsr     main_temp
    mov     volume_3,   main_temp
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
