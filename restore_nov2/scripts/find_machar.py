import os
for root, dirs, files in os.walk('.'):
    for f in files:
        if f.endswith(('.cpp', '.h', '.rc', '.md', '.txt')):
            p = os.path.join(root, f)
            with open(p, 'r', encoding='utf-8', errors='ignore') as fp:
                data = fp.read()
            c1 = data.count('抹茶')
            c2 = data.count('MIT')
            if c1 > 0 or c2 > 0:
                print(f"{p}: matcha={c1}, MIT={c2}")
