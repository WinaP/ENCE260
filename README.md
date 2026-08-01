# ENCE260 Assignment 2

Author : Wina Prasetyo, Shijie Ma

Date : 16/10/2025

## Description

This project implements a two-player game on the UCFK4 board. The game chosen for Group521 is a two-player maze game.

Each player uses their own board, and both boards generate the same random maze by exchanging a seed value via infrared (IR) communication.

Display: 5 columns × 7 rows (walls lit, free cells off)

Controls (navswitch): 
  - N/S/W/E move, 
  - PUSH confirm/start/new round

IR Communication: 
  - 'R' (ready)
  - 'W' (win)

Goal: Reach a section of the outer wall that is not lit (the exit).

### How it works
1. Both boards initialise and enter standby mode, continuously listening for IR signals.

2. When a player presses PUSH, the board:
    - Sends 'R' to indicate readiness.
    - Generates and transmits a random seed.

3. When a board receives 'R', it reconstructs the incoming seed from the following two bytes.

4. Once both boards are ready, each computes a shared seed using the bitwise XOR of its own seed and the received seed.
    - This ensures both boards generate identical mazes, with neither having full control of the seed.

5. The maze is generated using the shared seed.

6. The game begins after a 3–2–1 countdown.

7. Play the game while checking the IR receiver at all times.

8. When a player reaches the goal, their board sends 'W' to the other.

9. The game is stopped and the winner’s board displays “W”, while the other displays “L” for ~2 seconds.

10. After displaying the result, both boards reset and return to the standby state (Step 1).

### How to play
1. Both boards show “P” (press to start).

2. Each player presses PUSH to indicate readiness.

3. A 3-2-1 countdown appears.

4. Use N/S/E/W on the navswitch to move the blinking light (player position).

5. Reach the goal — an unlit section on the maze’s outer wall.

6. The first player to reach the goal sends 'W'; the other receives 'W' and shows “L”.

7. The result is displayed for ~2 seconds before going back to the 'P' state.

8. Press PUSH again to begin a new random round.

### Hardware Requirements
- 2 x UCFK4 boards (ATmega32U2)
- 2 x Micro USB cables

### Header Files used
* avr/io.h
* stdbool.h
* stdint.h
* stdlib.h

(Given in the ence-ucfk4 files)

* system.h
* ir_uart.h
* pio.h
* prescale.h
* timer.h
* usart1.h
* display.h
* ledmat.h
* navswitch.h
* font5x7_1.h
* pacer.h
* tinygl.h

### Files

game. c    ----> Main game file

Makefile   ----> Main Makefile

README.md  ----> Main README file

# GenAI Declaration 
We acknowledge that we have used Generative AI tools to support the development of this project such as ChatGPT and CoPilot. These tools were used to assist with the making and iteration of the game, the formatting and styling of codes, to debug and troubleshoot problems that arise, and to generate user-friendly and easy-to-understand comments. All content suggested by AI have been verified to be correct, real, relevant, and accurate. The use of GenAI complies with the University of Canterbury's academic integrity policy.