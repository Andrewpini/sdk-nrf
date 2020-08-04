import math
import random

EPOCH_AT_TAI_START = 946684800
TAI_START_YEAR = 2000
TM_START_YEAR = 1900
TAI_START_DAY = 6

MS_PER_SEC = 1000
DAYS_YEAR = 365
DAYS_LEAP_YEAR = 366
MS_PER_MIN = (60 * MS_PER_SEC)
MS_PER_HOUR = (60 * MS_PER_MIN)
MS_PER_DAY = (24 * MS_PER_HOUR)
MS_PER_YEAR = (DAYS_YEAR * MS_PER_DAY)
MS_PER_LEAP_YEAR = (DAYS_LEAP_YEAR * MS_PER_DAY)

MONTH_CFG = [31,28,31,30,31,30,31,31,30,31,30,31]
MONTH_LEAP_CFG = [31,29,31,30,31,30,31,31,30,31,30,31]
FEB_DAYS = 28
FEB_LEAP_DAYS = 29

dow = ("Søndag","Mandag","Tirsdag","Onsdag","Torsdag","Fredag","Lørdag")

def mhs_to_tai(hour,minute,sec):

        uptime = hour * MS_PER_HOUR
        uptime += minute * MS_PER_MIN
        uptime += sec * MS_PER_SEC
        return uptime


def tm_to_tai(year, day_of_year, hour, minute, sec):
        uptime = 0
        current_year = year

        while current_year > TAI_START_YEAR:
                current_year -= 1
                if (current_year % 4 is 0) and ((current_year % 100 is not 0) or (current_year % 400 is 0)):
                        uptime += MS_PER_LEAP_YEAR
                else:
                        uptime += MS_PER_YEAR
                # print("Uptime:{}".format(hex(uptime)))
        # print("STOP")
        uptime += day_of_year * MS_PER_DAY
        uptime += mhs_to_tai(hour,minute,sec)
        # print("Uptime:{}".format(uptime))
        return uptime




def tai_to_hms(uptime):
        uptime_rest = uptime % MS_PER_DAY
        hour = uptime_rest / MS_PER_HOUR
        print("Hours:{}".format(math.floor(hour)))
        minutes = (uptime % MS_PER_HOUR) / MS_PER_MIN
        print("Minutes:{}".format(math.floor(minutes)))
        seconds = (uptime % MS_PER_MIN) / MS_PER_SEC
        print("Seconds:{}".format(math.floor(seconds)))
        return hour, minutes, seconds

def tai_to_weekday(uptime):
        day = (TAI_START_DAY + (uptime / MS_PER_DAY)) % 7
        print("Day:{}".format(dow[math.floor(day)]))
        return day

def tai_to_year_conv(uptime):
        print(uptime)
        is_leap = False
        day_cnt = uptime / MS_PER_DAY
        current_year = TAI_START_YEAR
        current_month = 0
        while True:
                if (current_year % 4 is 0) and ((current_year % 100 is not 0) or (current_year % 400 is 0)):
                        if day_cnt >= DAYS_LEAP_YEAR:
                                day_cnt -= DAYS_LEAP_YEAR
                                current_year += 1
                        else:
                                is_leap = True
                                break
                else:
                        if day_cnt >= DAYS_YEAR:
                                day_cnt -= DAYS_YEAR
                                current_year += 1
                        else:
                                is_leap = False
                                break
        print("Year:{0}".format(current_year))
        print("Days since 1th of January:{0}".format((day_cnt)))

        day = day_cnt   # For test

        if is_leap:
                for month_day_cnt in MONTH_LEAP_CFG:
                        if day_cnt >= month_day_cnt:
                                day_cnt -= month_day_cnt
                                current_month += 1
                        else:
                                break
        else:
                for month_day_cnt in MONTH_CFG:
                        if day_cnt >= month_day_cnt:
                                day_cnt -= month_day_cnt
                                current_month += 1
                        else:
                                break
        print("Month:{0}".format(current_month))
        print("Day of the month:{0}".format(math.floor(day_cnt+1)))

        weekday = tai_to_weekday(uptime)
        hour, minutes, sec = tai_to_hms(uptime)
        # u_time = tm_to_tai(math.floor(current_year), math.floor(day), math.floor(hour), math.floor(minutes), math.floor(sec))

        # if u_time != uptime:
        #         raise Exception("ERROR")


def test(current_year):
        if (current_year % 4 is 0) and ((current_year % 100 is not 0) or (current_year % 400 is 0)):
                print(True)
        else:
                print(False)

# tai_to_year_conv(2.5 *MS_PER_YEAR)
# tai_to_hms(24*MS_PER_HOUR)
# tai_to_weekday(0)
# test(2004)
# tai_to_year_conv((366)*MS_PER_DAY+ (31+29+31+30)*MS_PER_DAY + (2*MS_PER_HOUR+63*MS_PER_MIN+61*MS_PER_SEC))
# tai_to_year_conv((365)*MS_PER_DAY)
# tai_to_year_conv(10000000000)
# tm_to_tai(2000, 115,17,46,40)
# tai_to_year_conv(1234)
# tm_to_tai(2000, 1,0,0,0)
# tai_to_year_conv(2345*MS_PER_DAY + 234*MS_PER_HOUR + 78*MS_PER_SEC)
# tm_to_tai(2006, 162,18,1,18)
# tai_to_year_conv(500000000000)



# for _ in range(1000):
#         nr = random.randint(0, 50000000000000)
#         nr = nr - nr % MS_PER_SEC
#         tai_to_year_conv(nr)

class UTC(object):
        def __init__(self, timestamp, curr, new):
                self.u_timestamp = timestamp
                self.u_curr = curr
                self.u_new = new

class Zone(object):
        def __init__(self, timestamp, curr, new):
                self.z_timestamp = timestamp
                self.z_curr = curr
                self.z_new = new

def alg_test(local_uptime, zone, utc):

        mod_uptime = local_uptime
        mod_uptime -= zone.z_curr
        mod_uptime += utc.u_curr
        # Greenwitch THAI

        if mod_uptime >= zone.z_timestamp:
                mod_uptime -= (zone.z_new - zone.z_curr)
                print("New Zone")

        if mod_uptime >= utc.u_timestamp:
                mod_uptime += (utc.u_new - utc.u_curr)
                print("New UTC")

        return mod_uptime

def alg_test2(uptime, zone, utc):

        comb1 = zone.z_curr - utc.u_curr
        comb2 = zone.z_curr - utc.u_new
        comb3 = zone.z_new - utc.u_curr
        comb4 = zone.z_new - utc.u_new

        if ((uptime - comb1) < utc.u_timestamp) and ((uptime - comb1) < zone.z_timestamp):
                print("Comb1 True. Uptime: {}".format(uptime - comb1))
        if ((uptime - comb2) >= utc.u_timestamp) and ((uptime - comb2) < zone.z_timestamp):
                print("Comb2 True. Uptime: {}".format(uptime - comb2))
        if ((uptime - comb3) < utc.u_timestamp) and ((uptime - comb3) >= zone.z_timestamp):
                print("Comb3 True. Uptime: {}".format(uptime - comb3))
        if ((uptime - comb4) >= utc.u_timestamp) and ((uptime - comb4) >= zone.z_timestamp):
                print("Comb4 True. Uptime: {}".format(uptime - comb4))


def test(year, day_of_year, hour, minute, sec):
        ZONE_FIRST = 2*MS_PER_HOUR
        ZONE_AFTER = 1*MS_PER_HOUR

        tai_at_changepoint = tm_to_tai(2020, 275, 2, 0, 0)
        zone_ctx = Zone(tai_at_changepoint, ZONE_FIRST, ZONE_AFTER)
        utc_ctx = UTC(tai_at_changepoint, 1000, 0)

        local_time = tm_to_tai(year, day_of_year, hour, minute, sec)
        res = alg_test(local_time, zone_ctx, utc_ctx)
        print("Result: {}".format(res))
        return res

# ZONE_FIRST = 2*MS_PER_HOUR
# ZONE_AFTER = 1*MS_PER_HOUR

# tai_at_changepoint = tm_to_tai(2020, 275, 2, 0, 0)

# zone_ctx = Zone(tai_at_changepoint, ZONE_FIRST, ZONE_AFTER)
# utc_ctx = UTC(tai_at_changepoint, 0, 1000)

# # Time the second before change
# result1 = alg_test(tai_at_changepoint + ZONE_FIRST - 1000, zone_ctx, utc_ctx)
# # Time at the change
# result2 = alg_test(tai_at_changepoint + ZONE_FIRST, zone_ctx, utc_ctx)
# # Time after change diff has elapsed
# result3 = alg_test(tai_at_changepoint + ZONE_AFTER-1000, zone_ctx, utc_ctx)

# print("Result1: {}, Result2: {}, Diff: {}".format(result1, result2, abs(result1 - result2)))

# print("\n")
# tai_to_year_conv(result1)
# print("\n")
# tai_to_year_conv(result2)
# print("\n")
# tai_to_year_conv(result3)

res= test(2020, 275, 3, 59, 58)
tai_to_year_conv(res)
print("\n")
res= test(2020, 275, 4, 0, 0)
tai_to_year_conv(res)
print("\n")
res= test(2020, 275, 5, 0, 0)
tai_to_year_conv(res)
print("\n")