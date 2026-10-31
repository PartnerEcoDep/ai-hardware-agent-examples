"""Keep reusable business/services/UI independent of platform and board headers."""
from pathlib import Path
import re

root = Path(__file__).resolve().parents[1]
errors = []
# Product build and test code must not require the donor example repository.
for folder in ('src', 'tests', 'skills/_shared/scripts'):
    for path in (root / folder).rglob('*'):
        if path.suffix not in ('.c', '.cpp', '.h', '.cmake', '.ps1') and path.name != 'CMakeLists.txt':
            continue
        if 'ai-hardware-agent-examples' in path.read_text(encoding='utf-8'):
            errors.append(f'{path.relative_to(root)} references the donor repository')
platform_headers = ('esp_', 'freertos/', 'driver/', 'board', 'ws_board',
                    'sdkconfig', 'goldie_', 'ws_', 'windows.h', 'wininet.h')
for folder in ('src/application/core', 'src/application/services', 'src/application/ui'):
    if not (root / folder).is_dir():
        errors.append(f'Missing portable source directory: {folder}')
    for path in (root / folder).rglob('*'):
        if path.suffix not in ('.c', '.h'):
            continue
        for name in re.findall(r'^\s*#\s*include\s*[<"]([^>"]+)', path.read_text(encoding='utf-8'), re.M):
            if name.startswith(platform_headers) or (folder != 'src/application/ui' and name.startswith('lvgl')):
                errors.append(f'{path.relative_to(root)} imports {name}')
# Board contracts must remain usable without platform service types.
for path in (root / 'src/boards').glob('*/include/*.h'):
    for name in re.findall(r'^\s*#\s*include\s*[<"]([^>"]+)', path.read_text(encoding='utf-8'), re.M):
        if name.startswith(('esp_', 'freertos/', 'goldie_', 'service_', 'driver_', 'audio_service', 'wifi_service')):
            errors.append(f'{path.relative_to(root)} exposes platform header {name}')
for path in (root / 'src/boards').rglob('*'):
    if path.suffix in ('.c', '.h'):
        source = path.read_text(encoding='utf-8')
        if re.search(r'\bwait_service\s*\(|#\s*include\s*"service_manager.h"', source):
            errors.append(f'{path.relative_to(root)} resolves a platform service')
# Media contracts/backends must not import cloud lifecycle or SDK interfaces.
for folder in ('src/application/services/media', 'src/platforms/esp_idf/components/media', 'src/platforms/goldieos/components/media'):
    for path in (root / folder).rglob('*'):
        if path.suffix not in ('.c', '.h', '.cpp', '.cc'):
            continue
        for name in re.findall(r'^\s*#\s*include\s*[<"]([^>"]+)', path.read_text(encoding='utf-8'), re.M):
            if name.startswith(('ai_cloud', 'convai_')):
                errors.append(f'{path.relative_to(root)} imports cloud/SDK header {name}')
# HAL implementation headers are private to that SDK module.
for path in (root / 'src/platforms').rglob('*'):
    if path.suffix not in ('.c', '.cpp', '.h'):
        continue
    source = path.read_text(encoding='utf-8')
    if 'convai_platform_esp32_internal.h' in source and 'components/convai' not in path.as_posix():
        errors.append(f'{path.relative_to(root)} imports SDK internals')
    if re.search(r'#\s*include\s*"(?:esp|ws)_services.h"', source):
        errors.append(f'{path.relative_to(root)} imports a removed aggregate header')
if errors:
    raise SystemExit('\n'.join(errors))
print('Architecture check passed: portable layers, board contracts, service ownership and SDK header boundaries.')
