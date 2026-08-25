"""
Simulering af et solcelleanlaeg med batteri.

Meningen er ikke at ramme fysikken praecist, men at give skaermen tal
der opfoerer sig som rigtige tal: sol der staar op og gaar ned, et
forbrug med morgen- og aftenspidser, og et batteri der lader op naar
der er overskud og aflader naar der mangler.

Fortegn, som resten af projektet bruger dem:
    battery_w   plus = aflader (leverer), minus = lader (forbruger)
    grid_w      plus = koeber fra nettet, minus = saelger til nettet

Sammenhaengen der skal holde, og som firmwaren regner den anden vej:
    inverter_ac = sol + batteri
    net         = forbrug - sol - batteri
    forbrug     = inverter_ac + net
"""

import math
import random


class Plant:
    def __init__(self,
                 has_battery: bool = True,
                 pv_peak_w: float = 7000.0,
                 pv_strings: int = 2,
                 battery_kwh: float = 10.24,
                 battery_max_w: float = 5000.0,
                 min_reserve_pct: float = 5.0,
                 seed: int = 1):
        self.has_battery = has_battery
        self.pv_peak_w = pv_peak_w
        self.pv_strings = max(1, pv_strings)
        self.battery_wh = battery_kwh * 1000.0
        self.battery_max_w = battery_max_w
        self.min_reserve_pct = min_reserve_pct

        self.soc_pct = 48.0
        self.rng = random.Random(seed)

        # Batteriet rammer ikke sit maal oejeblikkeligt. En rigtig
        # regulering bruger et halvt minuts tid paa at foelge med naar
        # forbruget springer, og i mellemtiden loeber forskellen ud paa
        # nettet. Uden den forsinkelse ville simulatoren holde nettet
        # paa praecis nul hele natten, og saa fik vi aldrig testet
        # hverken koeb, salg eller fortegnet paa NETTET-kortet.
        self._battery_actual_w = 0.0
        self.battery_tau_s = 90.0

        # Levetidstaellere, saa energiregistrene ikke bare staar paa nul.
        self.total_wh = 4_512_000.0

        self.solar_w = 0.0
        self.house_w = 0.0
        self.battery_w = 0.0
        self.grid_w = 0.0
        self.inverter_ac_w = 0.0
        self.string_w = [0.0] * self.pv_strings

        # Forbruget vandrer langsomt i stedet for at hoppe hvert sekund.
        self._house_drift = 0.0

    # ------------------------------------------------------------------

    def _solar_at(self, day_frac: float) -> float:
        """Solkurve. Nul foer kl. 5 og efter kl. 21, top ved middag."""
        hour = day_frac * 24.0
        if hour < 5.0 or hour > 21.0:
            return 0.0
        # Normaliser til 0..1 hen over de 16 lyse timer
        x = (hour - 5.0) / 16.0
        # sin^1.6 giver en pænere skulder end en ren sinus, taettere paa
        # hvordan en rigtig dag ser ud i SolarWeb.
        base = math.sin(math.pi * x) ** 1.6
        return max(0.0, self.pv_peak_w * base)

    def _house_at(self, day_frac: float) -> float:
        """Grundforbrug plus morgen- og aftenspids."""
        hour = day_frac * 24.0
        w = 280.0                                    # koeleskab, standby, router
        w += 1400.0 * math.exp(-((hour - 7.5) ** 2) / 2.0)    # morgen
        w += 2100.0 * math.exp(-((hour - 18.0) ** 2) / 3.0)   # aften
        return w

    def step(self, day_frac: float, dt_s: float):
        """Ét skridt frem. day_frac er 0..1 hen over doegnet."""
        # --- sol ---
        total_sun = self._solar_at(day_frac)
        if self.pv_strings == 1:
            self.string_w = [total_sun]
        else:
            # De to strenge vender lidt forskelligt, saa de ikke er ens.
            share = ([0.55, 0.45] + [0.0] * self.pv_strings)[: self.pv_strings]
            self.string_w = [total_sun * x for x in share]
        self.solar_w = sum(self.string_w)

        # --- forbrug ---
        self._house_drift += self.rng.uniform(-30.0, 30.0)
        self._house_drift = max(-250.0, min(250.0, self._house_drift))
        self.house_w = max(80.0, self._house_at(day_frac) + self._house_drift
                           + self.rng.uniform(-60.0, 60.0))

        # --- batteri ---
        if self.has_battery:
            surplus = self.solar_w - self.house_w
            target_w = 0.0
            if surplus > 50.0 and self.soc_pct < 99.9:
                target_w = -min(surplus, self.battery_max_w)
            elif surplus < -50.0 and self.soc_pct > self.min_reserve_pct:
                target_w = min(-surplus, self.battery_max_w)

            # Foerste ordens forsinkelse mod maalet.
            alpha = 1.0 - math.exp(-dt_s / self.battery_tau_s) if dt_s > 0 else 1.0
            self._battery_actual_w += (target_w - self._battery_actual_w) * alpha
            self.battery_w = self._battery_actual_w

            # Ladetilstanden integreres. 95 procents virkningsgrad hver vej.
            wh = self.battery_w * (dt_s / 3600.0)
            if self.battery_w < 0:
                wh *= 0.95            # noget gaar tabt paa vej ind
            else:
                wh /= 0.95            # og noget mere paa vej ud
            self.soc_pct -= (wh / self.battery_wh) * 100.0
            self.soc_pct = max(0.0, min(100.0, self.soc_pct))

            # Naar batteriet rammer en ende, holder det op af sig selv.
            if self.soc_pct <= self.min_reserve_pct and self.battery_w > 0:
                self.battery_w = 0.0
                self._battery_actual_w = 0.0
            if self.soc_pct >= 100.0 and self.battery_w < 0:
                self.battery_w = 0.0
                self._battery_actual_w = 0.0
        else:
            self.battery_w = 0.0
            self._battery_actual_w = 0.0

        # --- resten foelger af de tre ovenfor ---
        self.inverter_ac_w = self.solar_w + self.battery_w
        self.grid_w = self.house_w - self.solar_w - self.battery_w

        if self.inverter_ac_w > 0:
            self.total_wh += self.inverter_ac_w * (dt_s / 3600.0)

    # ------------------------------------------------------------------

    @property
    def charge_status(self) -> int:
        """SunSpec Model 124 ChaSt."""
        if not self.has_battery:
            return 1                      # OFF
        if self.soc_pct >= 99.9:
            return 5                      # FULL
        if self.soc_pct <= self.min_reserve_pct + 0.5:
            return 2                      # EMPTY
        if self.battery_w < -20.0:
            return 4                      # CHARGING
        if self.battery_w > 20.0:
            return 3                      # DISCHARGING
        return 6                          # HOLDING

    def summary(self) -> str:
        return (f"sol {self.solar_w:7.0f} W   "
                f"forbrug {self.house_w:6.0f} W   "
                f"batteri {self.battery_w:+7.0f} W  ({self.soc_pct:5.1f} %)   "
                f"net {self.grid_w:+7.0f} W")
