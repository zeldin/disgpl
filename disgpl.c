#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>

#define OP_IMM      (1U << 31)
#define OP_INDEXED  (1U << 30)
#define OP_VDP      (1U << 29)
#define OP_INDIRECT (1U << 28)
#define OP_GROM     (1U << 27)
#define OP_VREG     (1U << 26)

#define OP_INDEX(o)   (((o)>>16)&0xff)
#define OP_ADDRESS(o) ((o)&0xffff)

static uint32_t func_info[65536];

static size_t get_op(const uint8_t *p, uint32_t *op)
{
  uint32_t o = p[0];
  if (!(o&0x80)) {
    *op = o;
    return 1;
  }
  o = ((o & 0x70) << 24) | ((o&0xf) << 8);
  o |= p[1];
  size_t n = 2;
  if ((o & 0xf00) == 0xf00) {
    /* Extended */
    o = (o & ~0xffff) | ((o & 0xff) << 8) | p[2];
    n++;
  }
  if (o & OP_INDEXED)
    o |= p[n++] << 16;
  *op = o;
  return n;
}

static size_t get_imm(const uint8_t *p, uint32_t *op, bool w)
{
  if (w) {
    *op = OP_IMM | (p[0] << 8) | p[1];
    return 2;
  } else {
    *op = OP_IMM | p[0];
    return 1;
  }
}

static size_t get_moveop(const uint8_t *p, uint32_t *op, bool v, bool c)
{
  if (v)
    return get_op(p, op);
  size_t n = get_imm(p, op, true);
  if (c)
    *op |= (p[n++] << 16) | OP_INDEXED;
  *op = (*op & ~OP_IMM) | OP_GROM;
  return n;
}

static const char *type23_ops[] = {
  /* 0x00 */ "RTN",
  /* 0x01 */ "RTNC",
  /* 0x02 */ "*RAND",
  /* 0x03 */ "SCAN",
  /* 0x04 */ "*BACK",
  /* 0x05 */ "+B",
  /* 0x06 */ "+CALL",
  /* 0x07 */ "*ALL",
  /* 0x08 */ "FMT",
  /* 0x09 */ "H",
  /* 0x0a */ "GT",
  /* 0x0b */ "EXIT",
  /* 0x0c */ "CARRY",
  /* 0x0d */ "OVF",
  /* 0x0e */ "*PARSE",
  /* 0x0f */ "*XML",
  /* 0x10 */ "CONT",
  /* 0x11 */ "EXEC",
  /* 0x12 */ "RTNB",
  /* 0x13 */ "RTGR",
  /* 0x14 */ "XG-4",
  /* 0x15 */ "XG-5",
  /* 0x16 */ "XG-6",
  /* 0x17 */ "XG-7",
  /* 0x18 */ "XG-8",
  /* 0x19 */ "XG-9",
  /* 0x1a */ "XG-A",
  /* 0x1b */ "XG-B",
  /* 0x1c */ "XG-C",
  /* 0x1d */ "XG-D",
  /* 0x1e */ "XG-E",
  /* 0x1f */ "XG-$",
};

static const char *type4_ops[] = {
  /* 0x40-0x5f */ "BR",
  /* 0x60-0x7f */ "BS",
};

static const char *type5_ops[] = {
  /* 0x80-0x81 */ "ABS",
  /* 0x82-0x83 */ "NEG",
  /* 0x84-0x85 */ "INV",
  /* 0x86-0x87 */ "CLR",
  /* 0x88-0x89 */ "FETCH",
  /* 0x8a-0x8b */ "CASE",
  /* 0x8c-0x8d */ "PUSH",
  /* 0x8e-0x8f */ "CZ",
  /* 0x90-0x91 */ "INC",
  /* 0x92-0x93 */ "DEC",
  /* 0x94-0x95 */ "INCT",
  /* 0x96-0x97 */ "DECT",
  /* 0x98-0x99 */ "XG-0",
  /* 0x9a-0x9b */ "XG-1",
  /* 0x9c-0x9d */ "XG-2",
  /* 0x9e-0x9f */ "XG-3",
};

static const char *type1_ops[] = {
  /* 0xa0-0xa3 */ "ADD",
  /* 0xa4-0xa7 */ "SUB",
  /* 0xa8-0xab */ "MUL",
  /* 0xac-0xaf */ "DIV",
  /* 0xb0-0xb3 */ "AND",
  /* 0xb4-0xb7 */ "OR",
  /* 0xb8-0xbb */ "XOR",
  /* 0xbc-0xbf */ "ST",
  /* 0xc0-0xc3 */ "EX",
  /* 0xc4-0xc7 */ "CH",
  /* 0xc8-0xcb */ "CHE",
  /* 0xcc-0xcf */ "CGT",
  /* 0xd0-0xd3 */ "CGE",
  /* 0xd4-0xd7 */ "CEQ",
  /* 0xd8-0xdb */ "CLOG",
  /* 0xdc-0xdf */ "SRA",
  /* 0xe0-0xe3 */ "SLL",
  /* 0xe4-0xe7 */ "SRL",
  /* 0xe8-0xeb */ "SRC",
  /* 0xec-0xef */ "COINC",
  /* 0xf0-0xf3 */ "XG-F",
  /* 0xf4-0xf7 */ "I/O",
  /* 0xf8-0xfb */ "SWGR",
  /* 0xfc-0xff */ "XG-G",
};

static const char *fmt_op_low[] = {
  /* 0x00-0x1f */ "'HTEX",
  /* 0x20-0x3f */ "'VTEX",
  /* 0x40-0x5f */ "-HCHA",
  /* 0x60-0x7f */ "-VCHA",
  /* 0x80-0x9f */ "COL+",
  /* 0xa0-0xbf */ "ROW+",
  /* 0xc0-0xdf */ "RPTB",
  /* 0xe0-0xfa */ "+HSTR",
};

static const char *fmt_op_high[] = {
  /* 0xfb */ "@NEXT",
  /* 0xfc */ "-SCRO",
  /* 0xfd */ "+SCRO",
  /* 0xfe */ "-ROW",
  /* 0xff */ "-COL",
};

static void print_qstr(const uint8_t *p, unsigned l)
{
  while (l > 0) {
    if (*p < 0x20 || *p > 0x7e) {
      printf(">%02X", *p++);
      --l;
    } else {
      putchar('\'');
      while (l>0 && *p>=0x20 && *p<=0x7e) {
	putchar(*p++);
	--l;
      }
      putchar('\'');
    }
    if (l)
      printf(",");
  }
}

static void print_op(uint32_t o, bool w)
{
  if ((o & OP_IMM)) {
    if (w)
      printf(">%04X", (unsigned)OP_ADDRESS(o));
    else
      printf(">%02X", (unsigned)OP_ADDRESS(o));
  } else if ((o & OP_VREG)) {
    printf("R@%u", (unsigned)OP_ADDRESS(o));
  } else {
    if ((o & OP_VDP))
      printf("V");
    else if ((o & OP_GROM))
      printf("G");
    if ((o & OP_INDIRECT))
      printf("*");
    else
      printf("@");
    uint16_t a = OP_ADDRESS(o);
    if ((o & OP_INDIRECT) || !(o & (OP_VDP | OP_GROM)))
      a += 0x8300;
    printf(">%04X", (unsigned)a);
    if ((o & OP_INDEXED))
      printf("(@>%04X)", (unsigned)(0x8300U+OP_INDEX(o)));
  }
}

static void print_0op(const char *op)
{
  printf("%s\n", op);
}

static void print_1op(const char *op, uint32_t op1, bool w)
{
  if(w)
    printf("D%-5s", op);
  else
    printf("%-6s", op);
  print_op(op1, w);
  printf("\n");
}

static void print_2op(const char *op, uint32_t op1, uint32_t op2, bool w)
{
  if(w)
    printf("D%-5s", op);
  else
    printf("%-6s", op);
  print_op(op1, w);
  printf(",");
  print_op(op2, w);
  printf("\n");
}

static size_t disassemble_inst(uint16_t addr, const uint8_t *p)
{
  uint8_t op = *p;
  size_t n = 1;
  uint32_t src, dest;
  if (op & 0x80) {
    if (op < 0xa0) {
      /* type 5 */
      const char *opname = type5_ops[(op-0x80)>>1];
      bool w = op&1;
      n += get_op(p+n, &dest);
      print_1op(opname, dest, w);
      return n;
    } else {
      /* type 1 */
      const char *opname = type1_ops[(op-0xa0)>>2];
      bool w = op&1;
      bool u = (op>>1)&1;	
      n += get_op(p+n, &dest);
      n += (u? get_imm(p+n, &src, w) : get_op(p+n, &src));
      print_2op(opname, src, dest, w);
      return n;
    }
  } else if (op < 0x20) {
    const char *opname = type23_ops[op];
    if (opname[0] < '@') {
      /* type 2 */
      bool w = (opname[0] != '*');
      n += get_imm(p+n, &src, w);
      if (opname[0] == '+')
	src = (src & 0xffff) | OP_GROM;
      print_1op(opname+1, src, false);
      return n;
    } else {
      /* type 3 */
      print_0op(opname);
      return 1;
    }
  } else if (op < 0x40) {
    /* type 6 */
    uint32_t s2;
    bool g = (op >> 4)&1;
    bool r = (op >> 3)&1;
    bool v = (op >> 2)&1;
    bool c = (op >> 1)&1;
    if ((op&1))
      n += get_imm(p+n, &src, true);
    else
      n += get_op(p+n, &src);
    n += get_moveop(p+n, &dest, g, r);
    if (r && g)
      dest = OP_VREG | (dest & 7);
    n += get_moveop(p+n, &s2, v, c);
    printf("MOVE  ");
    print_op(src, true);
    printf(",");
    print_op(s2, false);
    printf(",");
    print_op(dest, false);
    printf("\n");
    return n;
  } else {
    /* type 4 */
    const char *opname = type4_ops[(op-0x40)>>5];
    uint32_t a = ((op & 0x1f) << 8) | p[1];
    a |= (addr & ~0x1fff) | OP_GROM;
    print_1op(opname, a, false);
    return 2;
  }
  printf("DATA  >%02X\n", op);
  return 1;
}

static size_t disassemble_fmt(uint16_t addr, const uint8_t *p)
{
  uint8_t op = *p;
  size_t n = 1;
  if (op < 0xfb) {
    const char *opname = fmt_op_low[op >> 5];
    uint8_t x = (op&0x1f)+1;
    if (opname[0] == '\'') {
      printf("%-6s", opname+1);
      print_qstr(p+n, x);
      printf("\n");
      return n+x;
    } else if (opname[0] < '@') {
      uint32_t src;
      if (opname[0] == '-')
	n += get_imm(p+n, &src, false);
      else
	n += get_op(p+n, &src);
      print_2op(opname+1, x|OP_IMM, src, false);
      return n;
    } else {
      print_1op(opname, x|OP_IMM, false);
      return n;
    }
  } else {
    const char *opname = fmt_op_high[op-0xfb];
    uint32_t src = 0;
    switch(opname[0]) {
    case '@':
      n += get_imm(p+n, &src, true);
      src = (src & 0xffff) | OP_GROM;
      break;
    case '-':
      n += get_imm(p+n, &src, false);
      break;
    case '+':
      n += get_op(p+n, &src);
      break;
    }
    print_1op(opname+1, src, false);
    return n;
  }
  printf("FDATA >%02X\n", op);
  return 1;
}

static size_t disassemble_data(uint16_t addr, const uint8_t *p, uint32_t l)
{
  size_t n = 0;
  printf("DATA  ");
  while (l > 0) {
    printf(">%02X", *p++);
    n++;
    if (--l)
      printf(",");
  }
  printf("\n");
  return n;
}

static void disassemble_region(uint16_t addr, const uint8_t *p, size_t l)
{
  size_t offs = 0;
  unsigned fmt = 0;
  uint32_t data_pending = 0;
  while (offs < l) {
    uint8_t op = p[offs];
    printf(">%04X : ", (unsigned)(addr+offs));
    if (data_pending) {
      size_t n = disassemble_data(addr+offs, p+offs, data_pending);
      offs += n;
      if (n >= data_pending)
	data_pending = 0;
      else
	data_pending -= n;
    } else if (fmt == 1 && op == 0xfb) {
      printf("FEND\n");
      fmt = 0;
      offs++;
    } else if (fmt) {
      if (op >= 0xc0 && op < 0xe0)
	fmt++;
      offs += disassemble_fmt(addr+offs, p+offs);
      if (op == 0xfb)
	--fmt;
    } else {
      if (op == 0x06) {
	uint16_t addr = (p[offs+1]<<8)|p[offs+2];
	data_pending = func_info[addr];
      }
      offs += disassemble_inst(addr+offs, p+offs);
      if (op == 0x08)
	fmt = 1;
    }
  }
}

int main(int argc, const char *argv[])
{
  if (argc < 3) {
    fprintf(stderr, "Usage: %s file base_addr [start_addr] [func_info_file]\n",
	    argv[0]);
    return 1;
  }
  unsigned long base = strtoul(argv[2], NULL, 16);
  unsigned long start = argc>3? strtoul(argv[3], NULL, 16) : base;
  FILE *f;
  if (argc>4) {
    if ((f = fopen(argv[4], "r"))) {
      unsigned addr, cnt;
      while (2 == fscanf(f, "%x : %u", &addr, &cnt)) {
	if (addr > 0xffff)
	  fprintf(stderr, "Bad address in %s\n", argv[4]);
	else
	  func_info[addr] = cnt;
      }
      fclose(f);
    } else fprintf(stderr, "Failed to open %s\n", argv[4]);
  }
  if ((f = fopen(argv[1], "r"))) {
    uint8_t buf[65536];
    size_t l = fread(buf, 1, sizeof(buf), f);
    fclose(f);
    if (start < base || start >= base+l) {
      fprintf(stderr, "Start address outside of image\n");
      return 1;
    }
    disassemble_region(start, buf+(start-base), l-(start-base));
  } else fprintf(stderr, "Failed to open %s\n", argv[1]);
  return 0;
}
