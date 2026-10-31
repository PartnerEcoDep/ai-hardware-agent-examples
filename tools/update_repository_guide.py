from pathlib import Path
import re,subprocess,os
r=Path(__file__).resolve().parents[1];os.chdir(r);p=r/'docs/repository-guide.md';s=p.read_text(encoding='utf-8')
files=set(subprocess.check_output(['git','-c','safe.directory='+r.as_posix(),'ls-files','-co','--exclude-standard','-z']).decode().split('\0'))
files={f for f in files if f and (r/f).is_file()}
pattern=r'^\| `([^`]+)` \| (\d+|—) \| ([^\n]+) \|$'
entries=[m.group(1) for m in re.finditer(pattern,s,re.M) if not m.group(3).isdigit()]
stale={f for f in entries if f not in files}
duplicates={f for f in entries if entries.count(f)>1}
assert not stale,('Stale or excluded entries',sorted(stale))
assert not duplicates,('Duplicate entries',sorted(duplicates))
listed=set(entries)
missing=files-listed
assert not missing,sorted(missing)
textfiles={f for f in files if not f.endswith(('.a','.tar','.tar.gz'))}
for i in range(4):
 p.write_text(s,encoding='utf-8')
 counts={f:len((r/f).read_text(encoding='utf-8').splitlines()) for f in textfiles}
 def row(m):
  f=m.group(1)
  if f not in files:return m.group(0)
  return f'| `{f}` | {counts.get(f,"—")} | {m.group(3)} |'
 s=re.sub(pattern,row,s,flags=re.M)
 dirs=set()
 for f in textfiles:
  for parent in Path(f).parents:
   if str(parent)!='.':dirs.add(parent.as_posix()+'/')
 table='| 目录 | 文本文件数（递归） | 行数（递归） |\n|---|---:|---:|\n'
 table+=f'| `./（项目合计）` | {len(textfiles)} | {sum(counts.values())} |\n'
 for d in sorted(dirs):
  child=[f for f in textfiles if f.startswith(d)]
  table+=f'| `{d}` | {len(child)} | {sum(counts[f] for f in child)} |\n'
 a=s.index('| 目录 | 文本文件数');b=s.index('\n根目录直属文本文件：',a)
 s=s[:a]+table+s[b:]
 root=[f for f in textfiles if '/' not in f]
 s=re.sub(r'根目录直属文本文件：[^\n]+',f'根目录直属文本文件：{len(root)} 个，共 {sum(counts[f] for f in root)} 行；已包含在项目合计中。',s)
p.write_text(s,encoding='utf-8')
print(f'Guide covers {len(files)} files, {len(textfiles)} text files; no unlisted source files.')
