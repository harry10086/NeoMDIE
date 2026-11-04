with open('src/core/GestureEngine.cpp', 'r', encoding='utf-8', errors='ignore') as f:
    lines = f.readlines()

for i, line in enumerate(lines):
    if 'CodeToArrowString' in line or 'ProcessMovement' in line:
        for j in range(i, min(i+35, len(lines))):
            s = ''.join(c if ord(c) < 128 else f'\\u{ord(c):04x}' for c in lines[j])
            print(f'{j+1}: {s}', end='')
        print('---')
