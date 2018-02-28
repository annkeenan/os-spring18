## README

## Contributors

Ann Keenan (akeenan2)

## Notes

To run the **hybrid version**, with the settings of curve B, which runs the command `./mandel -x 0.2869325 -y 0.0142905 -s .000001 -W 1024 -H 1024 -m 1000`, change the value of `MIN_ZOOM` to `0.000001` and modify the `buildCommand` function at line 101 in the code to the following:

```
void buildCommand(char **cmd, double s, int i) {
  char filename[13];
  char s_str[10];
  snprintf(filename, 13, "mandel%d.bmp", i);
  snprintf(s_str, 10, "%f", s);

  cmd[0] = strdup("./mandel");
  cmd[1] = strdup("-x");
  cmd[2] = strdup("0.2869325");
  cmd[3] = strdup("-y");
  cmd[4] = strdup("0.0142905");
  cmd[5] = strdup("-s");
  cmd[6] = strdup(s_str);
  cmd[7] = strdup("-m");
  cmd[8] = strdup("1000");
  cmd[9] = strdup("-W");
  cmd[10] = strdup("1024");
  cmd[11] = strdup("-H");
  cmd[12] = strdup("1024");
  cmd[13] = strdup("-o");
  cmd[14] = strdup(filename);
  cmd[15] = strdup("-n");
  cmd[16] = strdup("1");
  cmd[17] = NULL;
}
```

To run different numbers of threads, modify the `-n` flag by changing the value stored at `cmd[16]` to the number of threads desired.

The original code is as such:

```
void buildCommand(char **cmd, double s, int i) {
  char filename[13];
  char s_str[10];
  snprintf(filename, 13, "mandel%d.bmp", i);
  snprintf(s_str, 10, "%f", s);

  cmd[0] = strdup("./mandel");
  cmd[1] = strdup("-x");
  cmd[2] = strdup("0.2910234");
  cmd[3] = strdup("-y");
  cmd[4] = strdup("-0.0164365");
  cmd[5] = strdup("-s");
  cmd[6] = strdup(s_str);
  cmd[7] = strdup("-m");
  cmd[8] = strdup("4000");
  cmd[9] = strdup("-W");
  cmd[10] = strdup("700");
  cmd[11] = strdup("-H");
  cmd[12] = strdup("700");
  cmd[13] = strdup("-o");
  cmd[14] = strdup(filename);
  cmd[15] = strdup("-n");
  cmd[16] = strdup("1");
  cmd[17] = NULL;
}
```
