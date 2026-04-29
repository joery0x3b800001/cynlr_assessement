# generate_samples.py

import random

WIDTH = 1024
HEIGHT = 720
OUTPUT_FILE = "data/sample.csv"


def basic_noise():
  """Pure random noise (0–255)"""
  with open(OUTPUT_FILE, "w") as f:
      for _ in range(HEIGHT):
          row = [str(random.randint(0, 255)) for _ in range(WIDTH)]
          f.write(", ".join(row) + "\n")


def smooth_noise():
  """Correlated noise (less harsh, more natural)"""
  prev_row = [random.randint(0, 255) for _ in range(WIDTH)]

  with open(OUTPUT_FILE, "w") as f:
      for _ in range(HEIGHT):
          row = []
          for x in range(WIDTH):
              val = int((prev_row[x] + random.randint(0, 255)) / 2)
              row.append(str(val))
          prev_row = list(map(int, row))
          f.write(", ".join(row) + "\n")


def sensor_noise():
  """Simulates real sensor noise with bias + variation"""
  base = 120  # base brightness

  with open(OUTPUT_FILE, "w") as f:
      for _ in range(HEIGHT):
          row = [
              str(min(255, max(0, base + random.randint(-30, 30))))
              for _ in range(WIDTH)
          ]
          f.write(", ".join(row) + "\n")


if __name__ == "__main__":
  print("Select noise type:")
  print("1 - Basic Noise")
  print("2 - Smooth Noise")
  print("3 - Sensor-like Noise")

  choice = input("Enter choice (1/2/3): ").strip()

  if choice == "1":
      basic_noise()
  elif choice == "2":
      smooth_noise()
  elif choice == "3":
      sensor_noise()
  else:
      print("Invalid choice. Generating basic noise by default.")
      basic_noise()

  print(f"CSV file generated: {OUTPUT_FILE}")