import serial
import wave
import sys
import glob

if len(sys.argv) != 5:
    print('Usage: {} <serial port> <baud rate> <bytes to read> <label>'.format(sys.argv[0]))
    exit(0) 

port          = sys.argv[1]
baud          = sys.argv[2]
bytes_to_read = int(sys.argv[3])
label         = sys.argv[4] 

# find the maximum counter value to resume file naming
file_counts  = [int(item.split('.')[1])  for item in glob.glob('{}.*.wav'.format(label))]
count = 0
if len(file_counts) > 0:
    count = max(file_counts) + 1

print('Count started from {}'.format(count))

try :
    with serial.Serial(port, baud) as ser:
        while True:
            try:
                line = ser.readline().strip().decode("utf-8") 
                if line == 'START':
                    print('Start reading PCM data');
                    pcmdata = ser.read(bytes_to_read)
                    print('pcmdata bytes = {}'.format(len(pcmdata)))
                    filename = '{}.{:03d}.wav'.format(label, count)
                    with wave.open(filename, 'wb') as wavfile:
                        # params: mono (1) , 16bit (2 bytes), 16kHz sample rate,  compression NONE
                        wavfile.setparams((1, 2, 16000, 0, 'NONE', 'NONE'))
                        wavfile.writeframes(pcmdata)
                        count = count + 1
                        print('{} created.'.format(filename))
    
            except UnicodeDecodeError:
                pass
            except KeyboardInterrupt:
                print('\b\bExited.')
                break

except serial.serialutil.SerialException:
    print('Serial port: {} could not be opened.'.format(port))            
except ValueError:
    print('Baud rate: "{}" is not valid.'.format(baud))            
    
