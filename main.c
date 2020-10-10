#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>

#define pCS	10
#define pSK	14
#define pDI	12	// MOSI
#define pDO	13	// MISO

#define MFR_MAX_LEN 31
#define PRODUCT_MAX_LEN 31
#define SERIAL_MAX_LEN 13

// NOTE(lbayes): DELETEME
#define EEPROM_MODE_16BIT	2

void dump(char * title, int bits, uint16_t *dt, uint16_t n) {
  int clm = 0;
  uint16_t data;
  uint16_t saddr =0;
  uint16_t eaddr =n-1;

  printf("-------------------- [%s] --------------------\n", title);
  uint16_t addr;
  for (addr = saddr; addr <= eaddr; addr++) {
    data = dt[addr];
    if (clm == 0) {
      printf("%05x: ",addr);
    }

    if (bits == 8)  printf("%02x ",data);
    if (bits == 16) printf("%04x ",data);

    clm++;
    if (bits == 8 && clm == 16) {
      printf("\n");
      clm = 0;
    }
    if (bits == 16 && clm == 8) {
      printf("\n");
      clm = 0;
    }
  }
  printf("-------------------- [%s] --------------------\n", title);
}

/**
 * Read all bytes on the EEPROM and dump them to output.
void read_all(struct eeprom *dev) {
  uint16_t mem_addr;
  uint16_t data;
  uint16_t rdata[128];
  int i;

  memset(rdata, 0, sizeof(rdata));
  for(i=0;i<64;i++) {
    mem_addr = i;
    rdata[i] = eeprom_read(dev, mem_addr);
  }
  dump("16bit:address 0-64", 16, rdata, 64);
}
*/

typedef struct program_params {
  uint16_t vendor_id;
  uint16_t product_id;
  char *manufacturer; // limited to 31 bytes
  char *product; // limited to 31 bytes
  char *serial; // limited to 13 bytes
} program_params;

void exit_with_usage(char *argv[]) {
  fprintf(stderr, "Usage: %s [ \
      --vid [0x0000] \
      --pid [0x0000] \
      --manufacturer [31 chars max] \
      --product [31 chars max] \
      --serial [13 chars max]", argv[0]);
  exit(1);
}

void validate_params(char *argv[], program_params *params) {
  size_t len;

  len = strlen(params->manufacturer);
  if (len > MFR_MAX_LEN) {
    fprintf(stderr, "ERROR: Manfuacturer is too long at [%ld] bytes\n", len);
    exit_with_usage(argv);
  }

  len = strlen(params->product);
  if (len > PRODUCT_MAX_LEN) {
    fprintf(stderr, "ERROR: Product is too long at [%ld] bytes\n", len);
    exit_with_usage(argv);
  }

  len = strlen(params->serial);
  if (len > SERIAL_MAX_LEN) {
    fprintf(stderr, "ERROR: Serial is too long at [%ld] bytes\n", len);
    exit_with_usage(argv);
  }
}

program_params process_args(int argc, char *argv[]) {
  int c;
  int option_index = 0;
  int option_count = 0;
  int required_option_count = 5;
  program_params params = {};

  static struct option long_options[] = {
    {"vid", required_argument, NULL, 'v'},
    {"pid", required_argument, 0, 'p'},
    {"manufacturer", required_argument, 0, 'm'},
    {"product", required_argument, 0, 'r'},
    {"serial", required_argument, 0, 's'},
    {0, 0, 0, 0}
  };

  while (1) {
    c = getopt_long(argc, argv, "v:p:m:d:r:s:",
        long_options, &option_index);

    // We're done parsing args, bail and return
    if (c == -1) {
      break;
    }

    switch (c) {
      case 'v':
        params.vendor_id = strtol(optarg, NULL, 16);
        break;
      case 'p':
        params.product_id = strtol(optarg, NULL, 16);
        break;
      case 'm':
        params.manufacturer = optarg;
        break;
      case 'r':
        params.product = optarg;
        break;
      case 's':
        params.serial = optarg;
        break;
      default:
        exit_with_usage(argv);
    }

    option_count++;
  }

  if (option_count < (required_option_count)) {
    fprintf(stderr, "Usage: %s \
--vid [0x0000] \
--pid [0x0000] \
--manufacturer \"[31 chars max]\" \
--product \"[31 chars max]\" \
--serial \"[13 chars max]\"\n", argv[0]);
    exit(1);
  }

  validate_params(argv, &params);

  return params;
}

int main(int argc, char *argv[]) {
  program_params params = process_args(argc, argv);
  printf("-------------------------\n");
  printf("vid hex: 0x%x dec: %d\n", params.vendor_id, params.vendor_id);
  printf("pid hex: 0x%x dec: %d\n", params.product_id, params.product_id);
  printf("manufacturer: %s\n", params.manufacturer);
  printf("product: %s\n", params.product);
  printf("serial: %s\n", params.serial);

  // TODO(lbayes): UNCOMMENT FOLLOWING:
  // set EEPROM memory size
  // int eeprom_model = 46;
  // set EEPROM organization
  // int eeprom_org = EEPROM_MODE_16BIT;
  // int bits = 16;

  // start wiringPi
  // if (wiringPiSetup() == -1) {
    // printf("wiringPiSetup Error\n");
    // return 1;
  // }

  return 0;
}

/*
  // open device
  struct eeprom dev;
  int eeprom_bytes = eeprom_open(eeprom_model, eeprom_org, pCS, pSK, pDI, pDO, &dev);
  printf("EEPROM chip=93C%02d, %dBit Organization, Total=%dWords\n",eeprom_model, bits, eeprom_bytes);

  int is_enabled = eeprom_is_ew_enabled(&dev);
  printf("BEFORE EEPROM EW ENABLED? %d\n", is_enabled);
  eeprom_ew_enable(&dev);
  is_enabled = eeprom_is_ew_enabled(&dev);
  printf("AFTER EEPROM EW ENABLED? %d\n", is_enabled);

  // printf("ERASING ALL\n");
  eeprom_erase_all(&dev);

  uint16_t value;

  // Write first entry
  value = 0x6706;
  eeprom_write(&dev, 0x00, value);

  // WRITE ALL SLOTS?
  // eeprom_write_all(&dev, 0xbeef);

  // printf("WRITING VID / PID\n");
  // uint16_t vid = 0x335e;
  // uint16_t pid = 0x8a02;
  // eeprom_write(&dev, 0x01, vid);
  // eeprom_write(&dev, 0x02, pid);

  read_all(&dev);
  // if (eeprom_org == EEPROM_MODE_8BIT) {
  // printf("EEPROM chip=93C%02d, %dBit Organization, Total=%dBytes\n",eeprom_model, bits, eeprom_bytes);
  // } else {
  // }
  // if (eeprom_org == EEPROM_MODE_16BIT) org16Mode(&dev, eeprom_bytes);
*/
