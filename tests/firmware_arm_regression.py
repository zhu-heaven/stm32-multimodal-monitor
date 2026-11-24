import sys,struct
from pathlib import Path
project_root=Path(__file__).resolve().parents[1]/"firmware"
from unicorn import *
from unicorn.arm_const import *
from elftools.elf.elffile import ELFFile
with (project_root/'OBJ/LCD_ST7789.axf').open('rb') as f:
 e=ELFFile(f);segments=[(s['sh_addr'],s.data()) for s in e.iter_sections() if s['sh_flags']&2 and s['sh_type']=='SHT_PROGBITS'];syms={s.name:s['st_value'] for s in e.get_section_by_name('.symtab').iter_symbols()}
flashcalls=0

def emulator(saved=None):
 m=Uc(UC_ARCH_ARM,UC_MODE_THUMB|UC_MODE_MCLASS);m.mem_map(0x08000000,0x100000);m.mem_map(0x20000000,0x20000);m.mem_map(0x40020000,0x4000)
 for addr,data in segments:m.mem_write(addr,data)
 m.mem_write(0x08060000,saved if saved is not None else b'\xff'*0x20000)
 def hook(m,addr,size,u):
  global flashcalls
  for name in ['FLASH_Unlock','FLASH_Lock','FLASH_ClearFlag','FLASH_ProgramWord']:
   if addr==(syms[name]&~1):
    if name=='FLASH_ProgramWord':
     a=m.reg_read(UC_ARM_REG_R0);v=m.reg_read(UC_ARM_REG_R1);old=struct.unpack('<I',m.mem_read(a,4))[0];assert old&v==v;m.mem_write(a,struct.pack('<I',v));flashcalls+=1
    m.reg_write(UC_ARM_REG_R0,9);m.reg_write(UC_ARM_REG_PC,m.reg_read(UC_ARM_REG_LR));break
 m.hook_add(UC_HOOK_CODE,hook);return m
m=emulator()
def call(name,args=()):
 sp=0x2001f000;m.reg_write(UC_ARM_REG_SP,sp);m.reg_write(UC_ARM_REG_LR,0x080f0001)
 for reg,v in zip([UC_ARM_REG_R0,UC_ARM_REG_R1,UC_ARM_REG_R2,UC_ARM_REG_R3],args):m.reg_write(reg,v&0xffffffff)
 m.emu_start(syms[name]|1,0x080f0000,count=3000000);return m.reg_read(UC_ARM_REG_R0)
def text(addr):return bytes(m.mem_read(addr,200)).split(b'\0')[0].decode()
def apply(csv):m.mem_write(0x2001b000,csv.encode()+b'\0');return call('Risk_ApplyConfig',(0x2001b000,))
def active():call('Risk_ConfigText',(0x2001b200,));return text(0x2001b200)
base='1,0,32,28,20,12,8,50,200,500,300,600,800,200,500,800,20,50,150,200,500,1500'
call('Risk_Init');assert active()==base
updated=base.replace('1,0,32,28,20,12,8','1,1,100,0,0,0,0')
assert apply(updated)==1;assert active()==updated;writes=flashcalls;assert apply(updated)==1 and flashcalls==writes
assert apply(updated.replace('100,0,0,0,0','99,0,0,0,0'))==0
assert apply(updated.replace('50,200,500','200,50,500'))==0
assert apply(base)==0
assert apply(updated.replace('1,1,','1,2147483648,',1))==0
# Restart: retain only flash journal; reload all RAM from linked image.
saved=bytes(m.mem_read(0x08060000,0x20000));m=emulator(saved);call('Risk_Init');assert active()==updated
# Real compiled algorithm uses the edited weight: displacement band2 gives score2.00/danger.
taddr=0x2001c000
m.mem_write(taddr,struct.pack('<iiiiiiIBB',300,0,0,0,0,1000,1,1,1));assert call('Risk_SetBaseline',(taddr,))==1
for seq in [2,3]:
 m.mem_write(taddr,struct.pack('<iiiiiiIBB',300,0,0,0,0,1200,seq,1,1));call('Risk_Update',(taddr,0,0,0))
r=struct.unpack('<iiiiiHBBBB',m.mem_read(call('Risk_Get'),26));assert r[5]==200 and r[-2]==2,r
# Parser validates exact command field, rejects partial/bad strings.
idaddr,onaddr,configaddr=0x2001b400,0x2001b420,0x2001b500
for js,expected in [('{"id":"123","version":"1.0","params":{"risk_config":"'+updated+'"}}',2),('{"id":"124","params":{"buzzer_switch":true}}',1),('{"id":"125","params":{"risk_config":"1,2x"}}',-1)]:
 m.mem_write(0x2001b000,js.encode()+b'\0');ret=call('CloudCommand_Parse',(0x2001b000,idaddr,onaddr,configaddr));assert ret&0xffffffff==expected&0xffffffff,(ret,expected)
# Buzzer retains automatic priority.
call('Buzzer_SetAlarm',(2,));call('Buzzer_Set',(0,));assert call('Buzzer_Mode')==2
print('PASS: real ARM firmware parses configuration, rejects invalid/replayed input, saves atomically, restores after restart, applies edited weights and preserves alarm priority.')

if 'Accept' in syms:
 def accept(payload):
  checksum=0
  for c in payload.encode():checksum^=c
  m.mem_write(0x2001a000,(payload+'*%02X'%checksum).encode()+b'\0');return call('Accept',(0x2001a000,))
 stamp=1794933000
 assert accept('WX,-25,80,12,51,%d,%d'%(stamp,stamp))==1
 assert accept('WX,10,101,12,51,%d,%d'%(stamp,stamp))==0
 start=((stamp+28800)//86400-1)*86400-28800
 assert accept('WX3C,12,23,34,%d,%d,%d'%(start,stamp,stamp))==1
 assert call('Weather_Rain3d10')==69
 assert accept('WX3,12,23,34,%d,%d,%d'%(start+172800,stamp,stamp))==0
 assert accept('WX3C,12,23,34,%d,%d,%d'%(start+86400,stamp,stamp))==0
 assert accept('WX3C,2147483648,23,34,%d,%d,%d'%(start,stamp,stamp))==0
 print('PASS: compact weather parser retains checksum, signed temperature, range and rainfall-window validation.')
