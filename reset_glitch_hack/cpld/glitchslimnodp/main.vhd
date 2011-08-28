-- Xbox 360 slim reset glitch hack, 48Mhz clock + fake POST + i2c version
-- by GliGli

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

entity main is
  generic (                                          -- S >< E0                   >< ACK     >< CD                   ><   ACK   >< 04                   ><   ACK   >< 4E                   ><   ACK   >< 08                   ><   ACK   >< 80                   ><   ACK   >< 03                   ><   ACK   >< P
    SDA_SLOW_BITS : STD_LOGIC_VECTOR(255 downto 0) := b"1001111111110000000000000001111111111111111100000011111100011111111111111000000000000000111000000111111111110001110000001111111110001111111111100000000000011100000000011111111111111000000000000000000000111111111110000000000000000001111111111111111100101111";
         SCL_BITS : STD_LOGIC_VECTOR(255 downto 0) := b"1100100100100100100100100100001111100001001001001001001001001000011111000010010010010010010010010000111110000100100100100100100100100001111100001001001001001001001001000011111000010010010010010010010010000111110000100100100100100100100100001111100001111111";
                                                     -- S >< E0                   >< ACK     >< CD                   ><   ACK   >< 04                   ><   ACK   >< 4E                   ><   ACK   >< 80                   ><   ACK   >< 0c                   ><   ACK   >< 02                   ><   ACK   >< P
    SDA_FAST_BITS : STD_LOGIC_VECTOR(255 downto 0) := b"1001111111110000000000000001111111111111111100000011111100011111111111111000000000000000111000000111111111110001110000001111111110001111111111111100000000000000000000011111111111000000000000111111000000111111111110000000000000000001110001111111111100101111"
  );
  port (
    DBG : out STD_LOGIC := '0';
    POSTBIT : in STD_LOGIC;
    CLK : in STD_LOGIC;
    CPU_RESET : inout STD_LOGIC := 'Z';
    SDA : out  STD_LOGIC := 'Z';
    SCL : out  STD_LOGIC := 'Z'
  );
end main;

architecture counter of main is

constant I2C_CNT_WIDTH : integer := 15;
constant I2C_CLK_DIV : integer := 128;

constant CNT_WIDTH : integer := 16;
constant POSTCNT_WIDTH : integer := 4;

constant POST_B8 : integer := 10;
constant POST_BA : integer := 11;
constant POST_BB : integer := 12;

constant WIDTH_RESET_START  : integer := 17357; --17357; -- 8678
constant WIDTH_RESET_END    : integer := 2; --2; -- 1

constant TIME_RESET_START  : integer := WIDTH_RESET_START;
constant TIME_RESET_END    : integer := TIME_RESET_START+WIDTH_RESET_END;
constant TIME_BYPASS_END   : integer := 65535;

signal i2ccnt : integer range 0 to 2**I2C_CNT_WIDTH-1 := 2**I2C_CNT_WIDTH-1;
signal pslo : STD_LOGIC := '0';
signal slo : STD_LOGIC := '0';

signal cnt : integer range 0 to 2**CNT_WIDTH-1 := 0;
signal postcnt : integer range 0 to 2**POSTCNT_WIDTH-1 := 0;
signal pp: STD_LOGIC := '0';
signal ppp: STD_LOGIC := '0';
signal rst: STD_LOGIC := '0';

begin
  process(CLK, POSTBIT, CPU_RESET, postcnt, rst) is
  begin
    if CLK'event then
--    if rising_edge(CLK) then
      -- fake POST
      if (cnt = 0) and (CPU_RESET = '0') then
        postcnt <= 0;
        pp <= '0';
        ppp <= '0';
      else
        if ((postcnt = POST_B8) or (POSTBIT = ppp)) and ((POSTBIT xor pp) = '1') then -- detect POST changes / filter POST / don't filter glitch POST
          if postcnt<2**POSTCNT_WIDTH-1 then
            postcnt <= postcnt + 1;
          end if;
          pp <= POSTBIT;
        else
          ppp <= POSTBIT;
        end if;
      end if; 

      -- main counter
      if (postcnt < POST_BA) or (postcnt > POST_BB) then
        cnt <= 0;
      else
        if cnt<2**CNT_WIDTH-1 then
          cnt <= cnt + 1;
        end if;
      end if;
     
      -- slow flag
      if (postcnt >= POST_B8)  and (postcnt <= POST_BB) and (cnt < TIME_BYPASS_END) then
        slo <= '1';
      else
        slo <= '0';
      end if;
      
      -- reset
      if (cnt >= TIME_RESET_START) and (cnt < TIME_RESET_END) then
        rst <= '0';
      else
        rst <= '1';
      end if;
    end if;
    
    if rst = '0' then
      CPU_RESET <= '0';
    else
      CPU_RESET <= 'Z';
    end if;

    DBG <= slo;
  end process;

  -- i2c commands streamer
  process(CLK, slo, cnt) is
  begin
    if rising_edge(CLK) then
      if i2ccnt / I2C_CLK_DIV /= 255 then
        i2ccnt <= i2ccnt + 1;
        pslo <= slo;
      else
        if pslo /= slo then
          i2ccnt <= 0;
        end if;
      end if;
    end if;
    
    if ((slo = '1') and (SDA_SLOW_BITS(255 - i2ccnt / I2C_CLK_DIV) = '1')) or ((slo = '0') and (SDA_FAST_BITS(255 - i2ccnt / I2C_CLK_DIV) = '1')) then
      SDA <= 'Z';
    else
      SDA <= '0';
    end if;
      
    if SCL_BITS(255 - i2ccnt / I2C_CLK_DIV) = '1' then
      SCL <= 'Z';
    else
      SCL <= '0';
    end if;
    
  end process;

end counter;

