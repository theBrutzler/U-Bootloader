#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <penoid.h>

#define OLED_SDA 17
#define OLED_SCL 18
#define OLED_WIDTH 128
#define OLED_HEIGHT 64
#define COUNT 5

int x[COUNT];
int y[COUNT];
int dx[COUNT];
int dy[COUNT];

Adafruit_SSD1306 display(OLED_WIDTH, OLED_HEIGHT, &Wire);

void setup() {
  Wire.begin(OLED_SDA, OLED_SCL);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false);

  display.clearDisplay();
  display.setTextColor(WHITE);
  display.setCursor(0, 0);
  display.display();

  for (int i=0; i<COUNT; i++) {
    x[i]=i*(int)(OLED_WIDTH/COUNT);
    y[i]=random(OLED_HEIGHT-SIZE);
    dx[i] = random(2) ? 1 : -1;
    dy[i] = random(2) ? 1 : -1;
  }
}

void loop() {
  display.clearDisplay();
  for (int i=0; i<COUNT; i++) {
    for (int j = i + 1; j < COUNT; j++) {
      if (checkCollision(x[i], y[i], x[j], y[j])) {
        int tmpDx = dx[i];
        int tmpDy = dy[i];
        dx[i] = 2*dx[j];
        dy[i] = 2*dy[j];
        dx[j] = 2*tmpDx;
        dy[j] = 2*tmpDy;
      }
    }

    if (x[i] > OLED_WIDTH-SIZE) dx[i]=-1;
    if (x[i] < 0) dx[i]=1;
    if (y[i] > OLED_HEIGHT-SIZE) dy[i]=-1;
    if (y[i] < 0) dy[i]=1;

    x[i] = x[i] + dx[i];
    y[i] = y[i] + dy[i];
    display.drawBitmap(x[i], y[i], penoid, SIZE, SIZE, WHITE);
  }
  display.display();
  delay(10);
}

bool checkCollision(int x1, int y1, int x2, int y2) {
  return (x1 < x2 + SIZE &&
          x1 + SIZE > x2 &&
          y1 < y2 + SIZE &&
          y1 + SIZE > y2);
}