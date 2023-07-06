#ifndef _IL0323N_H
#define _IL0323N_H

#include <device.h>

/**
 * @brief Clears an area in the buffer
 * 
 * @param dev 
 * @param x 
 * @param y 
 * @param w 
 * @param h 
 */
void il0323_clear_area (const struct device *dev, uint8_t x, uint8_t y, uint8_t w, uint8_t h);

/**
 * @brief Sets a pixel in the buffer
 * 
 * @param dev 
 * @param x 
 * @param y 
 */
void il0323_set_pixel (const struct device *dev, uint16_t x, uint16_t y);

/**
 * @brief Clears a pixel in the buffer
 *
 * @param dev 
 * @param x 
 * @param y 
 */
void il0323_clear_pixel (const struct device *dev, uint8_t x, uint8_t y);

/**
 * @brief Refreshes an area of the screen
 * 
 * @param dev 
 * @param x 
 * @param y 
 * @param w 
 * @param h 
 * @return int 
 */
int il0323_refresh (struct device *dev, int16_t x, int16_t y, int16_t w, int16_t h);

/**
 * @brief Draws a horizontal line
 * 
 * @param dev 
 * @param x 
 * @param y 
 * @param len 
 */
void il0323_h_line (const struct device *dev, uint8_t x, uint8_t y, uint8_t len);

/**
 * @brief Draws a vertical line
 * 
 * @param dev 
 * @param x 
 * @param y 
 * @param len 
 */
void il0323_v_line (const struct device *dev, uint8_t x, uint8_t y, uint8_t len);

/**
 * @brief Hibernate display
 * 
 * @param driver 
 * @return int 
 */
int il0323_hibernate (const struct device *dev);

/**
 * @brief Initialize registers of il0323
 * 
 * @param dev 
 * @return int 
 */
int il0323_init_regs (const struct device *dev);

#endif