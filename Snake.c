#define F_CPU 16000000UL
#include <avr/eeprom.h>
#include <avr/interrupt.h>
#include <avr/io.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <util/delay.h>
#define COPI 5
#define SCK 7
#define SS 4
enum direction { UP, DOWN, RIGHT, LEFT };
uint8_t button_up = 0;
uint8_t button_down = 0;
uint8_t button_right = 0;
uint8_t button_left = 0;
enum direction current_direction = UP;
uint8_t snake_length = 1;
uint8_t snake[64][2];
enum direction snake_direction;
uint8_t food_location[2];
uint8_t screen[8];
uint8_t EEMEM TURN_ONS = 0;
void input_buttons_init(void) {
  DDRC = 0x00;
  PORTC = 0x0F;
}
void SPI_init(void) {
  DDRB = (1 << COPI) | (1 << SCK) | (1 << SS);
  SPCR = (1 << SPE) | (1 << MSTR);
}
void SPI_send(uint8_t register_address, uint8_t data) {
  PORTB &= ~(1 << SS);
  SPDR = register_address;
  while (!(SPSR & (1 << SPIF))) {
  }
  SPDR = data;
  while (!(SPSR & (1 << SPIF))) {
  }
  PORTB |= (1 << SS);
}
void LED_matrix_init(void) {
  SPI_send(0x09, 0x00);
  SPI_send(0x0A, 0x0F);
  SPI_send(0x0B, 0x07);
  SPI_send(0x0C, 0x01);
}
void timer0_init(void) {
  TIMSK |= 1 << TOIE0;
  TCCR0 = (1 << CS00) | (1 << CS01);
}
inline void button_update(uint8_t *button, uint8_t pin_value) {
  *button = (*button << 1) | pin_value;
}
void update_buttons_status(void) {
  button_update(&button_up, (~PINC & (1 << PINC0)) >> PINC0);
  button_update(&button_right, (~PINC & (1 << PINC1)) >> PINC1);
  button_update(&button_down, (~PINC & (1 << PINC2)) >> PINC2);
  button_update(&button_left, (~PINC & (1 << PINC3)) >> PINC3);
  if (button_up == 0xFF) {
    current_direction = UP;
  } else if (button_down == 0xFF) {
    current_direction = DOWN;
  } else if (button_right == 0xFF) {
    current_direction = RIGHT;
  } else if (button_left == 0xFF) {
    current_direction = LEFT;
  }
}
void snake_init(void) {
  snake[0][0] = rand() % 8;
  snake[0][1] = rand() % 8;
  snake_direction = UP;
}
uint8_t update_and_return_turn_ons(void) {
  uint8_t turn_ons = eeprom_read_byte(&TURN_ONS);
  turn_ons += 1;
  eeprom_update_byte(&TURN_ONS, turn_ons);
  return turn_ons;
}
void random_food_location(void) {
  bool food_snake_collision;
  do {
    food_snake_collision = false;
    food_location[0] = rand() % 8;
    food_location[1] = rand() % 8;
    for (uint8_t i = 0; i < snake_length; i++) {
      if (food_location[0] == snake[i][0] && food_location[1] == snake[i][1]) {
        food_snake_collision = true;
        break;
      }
    }
  } while (food_snake_collision);
}
inline int16_t modulo_8(int16_t x) { return (x % 8 + 8) % 8; }
void display(void) {
  for (uint8_t i = 0; i < 8; i++) {
    screen[i] = 0;
  }
  for (uint8_t i = 0; i < snake_length; i++) {
    screen[snake[i][0]] |= 1 << modulo_8(snake[i][1] - 1);
  }
  screen[food_location[0]] |= 1 << modulo_8(food_location[1] - 1);
  for (uint8_t i = 1; i < 9; i++) {
    SPI_send(i, screen[i - 1]);
  }
}
void wait(uint16_t ms) {
  while (ms--) _delay_ms(1);
}
bool has_snake_eaten_the_food(void) {
  switch (snake_direction) {
    case UP:
      return ((food_location[0] == snake[0][0]) &&
              (food_location[1] == modulo_8(snake[0][1] + 1)));
    case DOWN:
      return ((food_location[0] == snake[0][0]) &&
              (food_location[1] == modulo_8(snake[0][1] - 1)));
    case RIGHT:
      return ((food_location[0] == modulo_8(snake[0][0] - 1)) &&
              (food_location[1] == snake[0][1]));
    case LEFT:
      return ((food_location[0] == modulo_8(snake[0][0] + 1)) &&
              (food_location[1] == snake[0][1]));
    default:
      return false;
  }
}
void move_snake(void) {
  for (uint8_t i = snake_length - 1; i > 0; i--) {
    snake[i][0] = snake[i - 1][0];
    snake[i][1] = snake[i - 1][1];
  }
  switch (snake_direction) {
    case UP:
      snake[0][1] = modulo_8(snake[0][1] + 1);
      break;
    case DOWN:
      snake[0][1] = modulo_8(snake[0][1] - 1);
      break;
    case RIGHT:
      snake[0][0] = modulo_8(snake[0][0] - 1);
      break;
    case LEFT:
      snake[0][0] = modulo_8(snake[0][0] + 1);
      break;
    default:
      break;
  }
}
bool has_snake_collided_with_itself(void) {
  if (snake_length < 5) return false;
  for (uint8_t i = 4; i < snake_length; i++) {
    if ((snake[0][0] == snake[i][0]) && (snake[0][1] == snake[i][1]))
      return true;
  }
  return false;
}
void display_game_over_animation(void) {
  SPI_send(1, 0b00000001);
  SPI_send(2, 0b00000010);
  SPI_send(3, 0b01110100);
  SPI_send(4, 0b00000100);
  SPI_send(5, 0b00000100);
  SPI_send(6, 0b01110100);
  SPI_send(7, 0b00000010);
  SPI_send(8, 0b00000001);
  _delay_ms(1000);
  SPI_send(1, 0b00000001);
  SPI_send(2, 0b00100010);
  SPI_send(3, 0b00100100);
  SPI_send(4, 0b00100100);
  SPI_send(5, 0b00100100);
  SPI_send(6, 0b00100100);
  SPI_send(7, 0b00100010);
  SPI_send(8, 0b00000001);
  _delay_ms(1000);
}
ISR(TIMER0_OVF_vect) { update_buttons_status(); }
int main(void) {
  bool is_snake_dead = false;
  uint16_t speed_factor = 650;
  input_buttons_init();
  SPI_init();
  LED_matrix_init();
  timer0_init();
  snake_init();
  srand(update_and_return_turn_ons());
  random_food_location();
  sei();
  while (!is_snake_dead) {
    display();
    wait(speed_factor);
    if (!((snake_direction == current_direction) ||
          (snake_direction == UP && current_direction == DOWN) ||
          (snake_direction == LEFT && current_direction == RIGHT) ||
          (snake_direction == DOWN && current_direction == UP) ||
          (snake_direction == RIGHT && current_direction == LEFT))) {
      snake_direction = current_direction;
    }
    if (has_snake_eaten_the_food()) {
      speed_factor -= 10;
      snake_length += 1;
      move_snake();
      random_food_location();
    } else {
      move_snake();
    }
    if ((is_snake_dead = has_snake_collided_with_itself())) {
      display();
      _delay_ms(1000);
    }
  }
  while (true) {
    display_game_over_animation();
  }
  return EXIT_SUCCESS;
}