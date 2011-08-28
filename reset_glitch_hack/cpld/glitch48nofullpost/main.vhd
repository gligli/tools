-- Xbox 360 reset glitch hack, 48Mhz clock + fake POST version
-- by GliGli

library IEEE;
use IEEE.STD_LOGIC_1164.ALL;
use IEEE.NUMERIC_STD.ALL;

entity main is
  generic (
    POST_WIDTH : integer := 7
  );
  port (
    POSTBIT : in STD_LOGIC;
    CLK : in STD_LOGIC;
    CPU_PLL_BYPASS : out STD_LOGIC := '0';
    CPU_RESET : inout STD_LOGIC := 'Z'
    
--    TEST : out unsigned(4 downto 0)
  );
end main;

architecture counter of main is

constant CNT_WIDTH : integer := 16;
constant POSTCNT_WIDTH : integer := 8;

constant POST_37 : integer := 13;
constant POST_39 : integer := 14;
constant POST_3B : integer := 15;

constant WIDTH_RESET_START  : integer := 1628; -- zephyr: 1723, jasper: 1628
constant WIDTH_RESET_END    : integer := 5;
constant WIDTH_BYPASS_END   : integer := 48000;

constant TIME_RESET_START  : integer := WIDTH_RESET_START;
constant TIME_RESET_END    : integer := TIME_RESET_START+WIDTH_RESET_END;
constant TIME_BYPASS_END   : integer := TIME_RESET_END+WIDTH_BYPASS_END;

signal cnt : unsigned(CNT_WIDTH-1 downto 0);
signal postcnt : unsigned(POSTCNT_WIDTH-1 downto 0);
signal pp: STD_LOGIC := '0';
signal ppp: STD_LOGIC := '0';

begin
  process(CLK, POSTBIT, CPU_RESET, postcnt) is
  begin
--    TEST <= postcnt(TEST'range);
    
    if rising_edge(CLK) then
      -- fake POST
      if (to_integer(cnt) = 0) and (CPU_RESET = '0') then
        postcnt <= (others => '0');
        pp <= '0';
        ppp <= '0';
      else
        if ((to_integer(postcnt) = POST_37) or (POSTBIT = ppp)) and ((POSTBIT xor pp) = '1') then -- detect POST changes / filter POST / don't filter glitch POST
          postcnt <= postcnt + 1;
          pp <= POSTBIT;
        else
          ppp <= POSTBIT;
        end if;
      end if; 

      -- main counter
      if (to_integer(postcnt) < POST_39) or (to_integer(postcnt) > POST_3B) then
        cnt <= (others => '0');
      else
        if cnt<2**CNT_WIDTH-1 then
          cnt <= cnt + 1;
        end if;
      end if;
     
      -- bypass
      if (to_integer(postcnt) >= POST_37)  and (to_integer(postcnt) <= POST_3B) and (cnt < TIME_BYPASS_END) then
        CPU_PLL_BYPASS <= '1';
      else
        CPU_PLL_BYPASS <= '0';
      end if;
      
      -- reset
      if (cnt >= TIME_RESET_START) and (cnt < TIME_RESET_END) then
        CPU_RESET <= '0';
      else
        if (cnt >= TIME_RESET_END) and (cnt < TIME_BYPASS_END) then
          CPU_RESET <= '1';
        else
          CPU_RESET <= 'Z';
        end if;
      end if;
    end if;
    
  end process;
end counter;

