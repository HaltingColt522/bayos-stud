project('kernel', 'c',
  default_options: ['warning_level=2', 'buildtype=debug'],
)

nasm = find_program('nasm')

cc_args = [
  '-m64',
  '-fno-pie',
  '-fno-builtin',
  '-fno-exceptions',
  '-fno-unwind-tables',
  '-fno-asynchronous-unwind-tables',
  '-fno-ident',
  '-fno-stack-protector',
  '-mno-red-zone',
  '-mcmodel=kernel',
  '-mgeneral-regs-only',
  '-O1',
  '-nostdinc',
  '-ffreestanding',
  '-fno-stack-check',
  '-fno-lto',
  '-fno-PIC',
  '-ffunction-sections',
  '-fdata-sections',
  '-ggdb',
  '-fno-common',
  '-march=x86-64',
  '-mno-mmx',
  '-mno-sse',
]

deps_dir = meson.project_build_root() / 'deps'

run_command(
  find_program('sh'), files('scripts/fetch-deps.sh'), deps_dir,
  check: true,
)

hdrs_inc = deps_dir / 'freestanding-c-hdrs' / 'include'
limine_inc = deps_dir / 'limine-protocol' / 'include'
ccrt_src = deps_dir / 'cc-runtime' / 'src' / 'cc-runtime.c'

dep_hdrs = declare_dependency(compile_args: [
  '-isystem', hdrs_inc,
  '-isystem', limine_inc,
])

cc = meson.get_compiler('c')
ccrt_obj = custom_target('cc-runtime',
  output: 'cc-runtime.o',
  depfile: 'cc-runtime.o.d',
  command: cc.cmd_array() + cc_args + [
    '-isystem', hdrs_inc,
    '-MD', '-MF', '@DEPFILE@',
    '-c', ccrt_src,
    '-o', '@OUTPUT@',
  ],
)

inc = include_directories('include')

asm_srcs = files(
  'src/exception_handler.asm',
  'src/interrupt_handler.asm',
)

c_srcs = files(
  'src/framebuffer.c',
  'src/gdt.c',
  'src/interrupts.c',
  'src/io.c',
  'src/main.c',
  'src/memory.c',
  'src/pipe.c',
  'src/pmm.c',
  'src/printk.c',
  'src/sched.c',
  'src/sem.c',
  'src/vmm.c',
  'src/alloc.c',
  'src/kheap.c',
  'src/initramfs.c',
  'src/elf.c',
)

nasm_gen = generator(nasm,
  output: '@BASENAME@.o',
  arguments: ['-f', 'elf64', '@INPUT@', '-o', '@OUTPUT@'],
)

asm_objs = nasm_gen.process(asm_srcs)

font_obj = files('assets/font.o')

kernel_ld = files('linker-scripts/x86_64.lds')

kernel = executable('kernel.elf',
  sources: c_srcs,
  objects: [asm_objs, font_obj, ccrt_obj],
  c_args: cc_args,
  dependencies: dep_hdrs,
  include_directories: inc,
  link_args: [
    '-T', kernel_ld[0].full_path(),
    '-nostdlib',
    '-no-pie',
    '-Wl,--gc-sections',
  ],
  link_depends: kernel_ld,
  install: true,
  install_dir: 'boot',
)
