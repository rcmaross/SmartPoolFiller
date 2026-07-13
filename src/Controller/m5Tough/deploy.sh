cp ./build/esp32.esp32.m5stack_tough/m5Tough.ino.bin bin/
python3 ~/Library/Arduino15/packages/esp32/hardware/esp32/*/tools/espota.py -i SmartPoolFiller-Tough.local -p 3232 -f ./bin/m5Tough.ino.bin -a admin

