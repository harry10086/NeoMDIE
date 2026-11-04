import os

for root, dirs, files in os.walk('src'):
    for f in files:
        if f.endswith(('.cpp', '.h', '.rc')):
            p = os.path.join(root, f)
            with open(p, 'r', encoding='utf-8', errors='ignore') as fp:
                lines = fp.readlines()
            for idx, line in enumerate(lines):
                if '分页' in line:
                    print(f"{p}:{idx+1}: {line.strip()}")
