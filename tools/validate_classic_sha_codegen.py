"""Execute emitted Xtensa kernel instructions against a strict SHA MMIO model.

This is an instruction-level host model, NOT an ESP32 emulator or hardware
benchmark. It checks register allocation, byte order, padding, the idle-before-
write contract, protected DPORT reads, and the actual compiled early filter.
"""
import argparse
import hashlib
import random
import re
import struct
import subprocess
from pathlib import Path


def output(command):
    return subprocess.check_output(command, text=True)


class Kernel:
    TEXT = 0x3ff03000
    APB = 0x3ff40078

    def __init__(self, elf, toolchain):
        self.image = elf.read_bytes()
        h = struct.unpack('<16sHHIIIIIHHHHHH', self.image[:52])
        self.sections = []
        for i in range(h[12]):
            s = struct.unpack_from('<10I', self.image, h[6] + i * h[11])
            if s[1] == 1:
                self.sections.append((s[3], s[4], s[5]))
        nm = output([str(toolchain / 'xtensa-esp32-elf-nm.exe'), '-S', '-C', str(elf)])
        self.names = {}
        helper_ranges = []
        for line in nm.splitlines():
            parts = line.split(maxsplit=3)
            if len(parts) == 3 and re.fullmatch('[0-9a-f]+', parts[0]):
                self.names[int(parts[0], 16)] = parts[2]
            if len(parts) == 4:
                self.names[int(parts[0], 16)] = parts[3]
                if 'runClassicHardwareSequential(' in parts[3]:
                    self.start = int(parts[0], 16)
                    self.size = int(parts[1], 16)
                if 's_working_generation' in parts[3]:
                    self.generation = int(parts[0], 16)
                if 'recordHardwareCandidate(' in parts[3]:
                    helper_ranges.append((int(parts[0], 16), int(parts[1], 16)))
        listing = output([str(toolchain / 'xtensa-esp32-elf-objdump.exe'), '-d', '-C',
                          f'--start-address={self.start}',
                          f'--stop-address={self.start + self.size}', str(elf)])
        for address, size in helper_ranges:
            listing += output([str(toolchain / 'xtensa-esp32-elf-objdump.exe'), '-d', '-C',
                               f'--start-address={address}', f'--stop-address={address + size}', str(elf)])
        self.code = {}
        for line in listing.splitlines():
            m = re.match(r'^\s*([0-9a-f]+):\s+([0-9a-f]+)\s+(\S+)\s*(.*)', line)
            if m and m[3] != '.byte':
                self.code[int(m[1], 16)] = (m[3].removesuffix('.n'),
                    [p.strip() for p in m[4].split('<')[0].split(',') if p.strip()],
                    len(m[2]) // 2)

    def read(self, address):
        if address == self.APB:
            self.preread = True
            return 0
        if self.TEXT <= address <= self.TEXT + 0x9c:
            assert self.other_cpu_stalled or (self.preread and (self.ps & 15) >= 5), 'unprotected DPORT read'
            self.preread = False
            if address == self.TEXT + 0x9c:
                assert not self.command_needs_barrier, 'BUSY read before command MEMW barrier'
                if self.busy:
                    self.busy -= 1
                    return 1
                return 0
            assert not self.busy, 'digest read before idle'
            assert self.memory_locked, 'digest read without shared-memory lock'
            self.digest_reads += 1
            return self.text[(address - self.TEXT) // 4]
        if address in self.memory:
            if address == self.generation and self.cancel_after is not None and len(self.completed) > self.cancel_after:
                return 20
            return self.memory[address]
        for base, offset, size in self.sections:
            if base <= address < base + size:
                return struct.unpack_from('<I', self.image, offset + address - base)[0]
        raise AssertionError(f'uninitialized read {address:x}')

    def write(self, address, value):
        value &= 0xffffffff
        if self.TEXT <= address < self.TEXT + 64:
            assert self.memory_locked, 'SHA_TEXT access without shared-memory lock'
            assert not self.busy, 'SHA_TEXT written while BUSY'
            self.text[(address - self.TEXT) // 4] = value
            self.writes += 1
        elif address in (self.TEXT + 0x90, self.TEXT + 0x94, self.TEXT + 0x98):
            assert not self.busy and value == 1, 'invalid engine command'
            self.controls.append(address - self.TEXT)
            block = struct.pack('>16I', *self.text)
            if address == self.TEXT + 0x90 and self.phase in (0, 5):
                self.first = block
                self.phase = 1
            elif address == self.TEXT + 0x94 and self.phase == 1:
                assert block[16:] == b'\x80' + bytes(39) + (640).to_bytes(8, 'big')
                self.header = self.first + block[:16]
                self.digest = hashlib.sha256(self.header).digest()
                self.phase = 2
            elif address == self.TEXT + 0x98 and self.phase in (2, 4):
                self.text[:8] = struct.unpack('>8I', self.digest)
                if self.phase == 4: self.completed.append((self.header, self.digest))
                self.phase += 1
            elif address == self.TEXT + 0x90 and self.phase == 3:
                assert block[32:] == b'\x80' + bytes(23) + (256).to_bytes(8, 'big')
                assert block[:32] == self.digest
                self.digest = hashlib.sha256(block[:32]).digest()
                self.phase = 4
            else:
                raise AssertionError('wrong SHA operation order')
            self.busy = self.delay
            self.command_needs_barrier = True
        else:
            self.memory[address] = value

    def read_byte(self, address):
        return (self.read(address & ~3) >> (8 * (address & 3))) & 255

    def write_byte(self, address, value):
        base, shift = address & ~3, 8 * (address & 3)
        old = self.memory.get(base, 0)
        self.memory[base] = (old & ~(255 << shift)) | ((value & 255) << shift)

    def run(self, header, delay=2, initial_level=0, nonces=1, cancel_after=None):
        self.memory = {self.generation: 19}
        self.text = [0] * 16
        self.busy = self.phase = self.writes = self.digest_reads = 0
        self.memory_locked = False
        self.other_cpu_stalled = False
        self.command_needs_barrier = False
        self.delay = delay
        self.preread = False
        self.ps = 0x40000 | initial_level
        initial_ps = self.ps
        self.controls = []
        self.completed = []
        self.cancel_after = cancel_after
        self.registers = [0] * 16
        r = self.registers
        r[1:6] = [0x20010000, 0x20000000, 0x20001000, 0x20002000, 0x20003000]
        self.memory[r[2]] = 19
        self.memory[r[2] + 4] = int.from_bytes(header[76:80], 'little')
        self.memory[r[2] + 8] = nonces
        self.memory[r[2] + 16] = self.memory[r[2] + 20] = 0  # Job-owned threshold: 0.0.
        self.memory[r[3] + 16] = self.memory[r[3] + 20] = 0
        self.memory[r[3] + 136] = 0
        for i in range(20):
            self.memory[r[4] + 4*i] = int.from_bytes(header[4*i:4*i+4], 'big')
            self.memory[r[2] + 252 + 4*i] = int.from_bytes(header[4*i:4*i+4], 'little')
        pc, instructions = self.start, 0
        call_stack = []
        def reg(arg):
            return r[int(arg[1:])]
        def branch(arg):
            return int(arg.split()[0], 16)
        while True:
            op, a, length = self.code[pc]
            pc += length
            instructions += 1
            assert instructions < 10000 * nonces, 'kernel failed to terminate'
            value = None
            if op == 'entry': r[1] -= int(a[1], 0)
            elif op == 'retw':
                if not call_stack:
                    break
                returned = r[2:4]
                pc, parent = call_stack.pop()
                r[:] = parent
                r[10:12] = returned
            elif op == 'memw': self.command_needs_barrier = False
            elif op in ('rsync', 'nop'): pass
            elif op == 'l32r': value = self.read(branch(a[1]))
            elif op == 'l32i': value = self.read(reg(a[1]) + int(a[2], 0))
            elif op == 's32i': self.write(reg(a[1]) + int(a[2], 0), reg(a[0]))
            elif op == 'l8ui': value = self.read_byte(reg(a[1]) + int(a[2], 0))
            elif op == 's8i': self.write_byte(reg(a[1]) + int(a[2], 0), reg(a[0]))
            elif op == 'movi': value = int(a[1], 0)
            elif op == 'mov': value = reg(a[1])
            elif op == 'addi': value = reg(a[1]) + int(a[2], 0)
            elif op == 'add': value = reg(a[1]) + reg(a[2])
            elif op == 'and': value = reg(a[1]) & reg(a[2])
            elif op == 'or': value = reg(a[1]) | reg(a[2])
            elif op == 'srli': value = reg(a[1]) >> int(a[2], 0)
            elif op == 'slli': value = reg(a[1]) << int(a[2], 0)
            elif op == 'extui': value = (reg(a[1]) >> int(a[2], 0)) & ((1 << int(a[3], 0)) - 1)
            elif op == 'rsil': value, self.ps = self.ps, (self.ps & ~15) | int(a[1], 0)
            elif op == 'wsr.ps': self.ps = reg(a[0])
            elif op == 'bnez':
                if reg(a[0]): pc = branch(a[1])
            elif op == 'beqz':
                if not reg(a[0]): pc = branch(a[1])
            elif op == 'blti':
                signed = reg(a[0]) if reg(a[0]) < 0x80000000 else reg(a[0]) - 0x100000000
                if signed < int(a[1], 0): pc = branch(a[2])
            elif op == 'bgei':
                signed = reg(a[0]) if reg(a[0]) < 0x80000000 else reg(a[0]) - 0x100000000
                if signed >= int(a[1], 0): pc = branch(a[2])
            elif op in ('bltu', 'bgeu', 'bne', 'beq'):
                if op == 'bltu': take = reg(a[0]) < reg(a[1])
                elif op == 'bgeu': take = reg(a[0]) >= reg(a[1])
                elif op == 'bne': take = reg(a[0]) != reg(a[1])
                else: take = reg(a[0]) == reg(a[1])
                if take: pc = branch(a[2])
            elif op == 'j': pc = branch(a[0])
            elif op in ('call8', 'callx8'):
                target = branch(a[0]) if op == 'call8' else reg(a[0])
                name = self.names[target]
                if name.startswith('(anonymous namespace)::recordHardwareCandidate('):
                    parent = r.copy()
                    call_stack.append((pc, parent))
                    r[:] = [0, parent[1]] + parent[10:16] + [0] * 8
                    pc = target
                elif name == '__bswapsi2':
                    r[10] = int.from_bytes(r[10].to_bytes(4, 'little'), 'big')
                elif name == 'esp_dport_access_sequence_reg_read':
                    self.preread = True
                    r[10] = self.read(r[10])
                elif name == 'esp_dport_access_reg_read':
                    # SDK routine: RSIL 5, APB pre-read, DPORT load, restore PS.
                    saved_ps = self.ps
                    self.ps = (self.ps & ~15) | 5
                    self.preread = True
                    r[10] = self.read(r[10])
                    self.ps = saved_ps
                elif name == '_xtos_set_intlevel':
                    self.ps = (self.ps & ~15) | (r[10] & 15)
                elif name.startswith('diff_from_target('):
                    actual = b''.join(self.memory[0x20003000 + 4*i].to_bytes(4, 'little') for i in range(8))
                    assert actual == self.digest, 'wrong emitted full digest / endian conversion'
                    assert self.digest[-2:] == b'\0\0', 'false candidate acceptance'
                    # Exercise storage/ownership, not floating-point difficulty
                    # arithmetic (the native suite validates the target gate).
                    r[10], r[11] = 0, 0x3ff00000  # 1.0 > initial 0.0
                elif name in ('__gtdf2', '__gedf2', '__ledf2', '__ltdf2'):
                    left = struct.unpack('<d', struct.pack('<II', r[10], r[11]))[0]
                    right = struct.unpack('<d', struct.pack('<II', r[12], r[13]))[0]
                    r[10] = ((left > right) - (left < right)) & 0xffffffff
                elif name == 'esp_sha_lock_memory_block':
                    assert not self.memory_locked
                    self.memory_locked, self.memory_ps = True, self.ps
                    self.ps = (self.ps & ~15) | 3
                elif name == 'esp_sha_unlock_memory_block':
                    assert self.memory_locked and not self.busy and not self.other_cpu_stalled
                    self.memory_locked, self.ps = False, self.memory_ps
                elif name == 'esp_ipc_isr_stall_other_cpu':
                    assert self.memory_locked and not self.other_cpu_stalled
                    self.other_cpu_stalled = True
                elif name == 'esp_ipc_isr_release_other_cpu':
                    assert self.memory_locked and self.other_cpu_stalled and not self.busy
                    self.other_cpu_stalled = False
                elif name == 'sha_hal_wait_idle':
                    assert self.memory_locked and not self.busy
                elif name.startswith('isSha256Valid('): r[10] = int(any(self.digest))
                elif name == 'memcpy':
                    data = [self.read_byte(r[11] + i) for i in range(r[12])]
                    for i, byte in enumerate(data): self.write_byte(r[10] + i, byte)
                else: raise AssertionError(f'unexpected call {name}')
            else: raise AssertionError(f'unsupported instruction {op} {a}')
            if value is not None: r[int(a[0][1:])] = value & 0xffffffff
        expected_count = nonces if cancel_after is None else min(nonces, ((cancel_after + 255) // 256) * 256 + 1)
        for i in range(expected_count):
            candidate_header = header[:76] + ((int.from_bytes(header[76:80], 'little') + i) & 0xffffffff).to_bytes(4, 'little')
            if hashlib.sha256(hashlib.sha256(candidate_header).digest()).digest()[-2:] == b'\0\0':
                expected_count = i + 1
                break
        assert len(self.completed) == expected_count
        hits = []
        start_nonce = int.from_bytes(header[76:80], 'little')
        for i, (actual_header, digest) in enumerate(self.completed):
            expected_header = header[:76] + ((start_nonce + i) & 0xffffffff).to_bytes(4, 'little')
            expected = hashlib.sha256(hashlib.sha256(expected_header).digest()).digest()
            assert actual_header == expected_header and digest == expected, 'wrong nonce/header'
            if expected[-2:] == b'\0\0': hits.append(expected_header)
        assert self.controls == [0x90, 0x94, 0x98, 0x90, 0x98] * expected_count
        assert self.writes == 40 * expected_count and self.ps == initial_ps
        assert not self.other_cpu_stalled and not self.memory_locked
        assert not self.memory_locked
        hit = bool(hits)
        assert self.digest_reads == expected_count + 7 * len(hits), 'wrong filter branch'
        assert self.memory[0x20001000 + 8] == expected_count, 'wrong completed-nonce count'
        assert self.read_byte(0x20001000 + 136) == int(hit), 'wrong candidate presence'
        if hit:
            saved = bytes(self.read_byte(0x20001000 + 56 + i) for i in range(80))
            assert saved in hits, 'candidate header/nonce ownership mismatch'
            assert self.memory[0x20001000 + 4] == int.from_bytes(saved[76:80], 'little')
        return instructions, hit


def main():
    p = argparse.ArgumentParser()
    p.add_argument('elf', type=Path)
    p.add_argument('toolchain', type=Path)
    p.add_argument('--cases', type=int, default=10000)
    args = p.parse_args()
    kernel = Kernel(args.elf, args.toolchain)
    rng = random.Random(0x4e657264)
    headers = []
    vectors = Path(__file__).resolve().parents[1] / 'test/native_mining_validation.cpp'
    for match in re.finditer(r'(?:requireDoubleDigest\(|decodeHex\()\s*((?:"[0-9a-f]+"\s*)+)', vectors.read_text()):
        text = ''.join(re.findall(r'"([0-9a-f]+)"', match[1]))
        if len(text) == 160: headers.append(bytes.fromhex(text))
    for nonce in (0, 1, 0xff, 0x100, 0xffff, 0x10000, 0x7fffffff, 0x80000000, 0xfffffffe, 0xffffffff):
        headers.append(bytes(76) + nonce.to_bytes(4, 'little'))
    headers.extend(rng.randbytes(80) for _ in range(args.cases))
    # A genuine early-filter hit with nonce UINT32_MAX checks the old sentinel
    # defect using real SHA-256d, not a forged digest or a modified filter.
    for salt in range(2000000):
        header = salt.to_bytes(4, 'little') + bytes(72) + b'\xff' * 4
        if hashlib.sha256(hashlib.sha256(header).digest()).digest()[-2:] == b'\0\0':
            headers.append(header)
            break
    else: raise AssertionError('failed to construct nonce-boundary filter hit')
    hits = total = 0
    for i, header in enumerate(headers):
        count, hit = kernel.run(header, i % 5, i % 5)
        total += count
        hits += hit
    for count, cancel_after in ((4096, None), (16384, None), (1024, 0), (1024, 30), (1024, 256)):
        header = bytes(76) + (0xfffff000).to_bytes(4, 'little')
        count_instructions, _ = kernel.run(header, 2, 0, count, cancel_after)
        print(f'SIMULATED range: requested={count}, cancel_after={cancel_after}, completed={len(kernel.completed)}, instructions={count_instructions}')
    # A known real Bitcoin hit inside a range must terminate only its prefix;
    # the next invocation must resume the suffix without losing/counting twice.
    original = headers[0]
    start = (int.from_bytes(original[76:80], 'little') - 1) & 0xffffffff
    done = 0
    while done < 32:
        header = original[:76] + ((start + done) & 0xffffffff).to_bytes(4, 'little')
        _, hit = kernel.run(header, 2, 0, 32 - done)
        if done == 0: assert hit and len(kernel.completed) == 2
        done += len(kernel.completed)
    assert done == 32
    print('SIMULATED candidate prefix/suffix: 32 unique nonces, actual historical hit retained.')
    print(f'SIMULATED emitted Xtensa kernel: {len(headers)} headers, {hits} full-digest hits, zero mismatches.')
    print('40 writes / 3 compressions / 2 LOADs; idle-before-write and APB/interrupt protection passed.')
    print(f'Instructions exercised: {total}; no hardware cycle/throughput claim.')


if __name__ == '__main__':
    main()
