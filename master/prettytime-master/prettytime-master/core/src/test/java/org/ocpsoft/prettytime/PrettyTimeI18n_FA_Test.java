/*
 * Copyright 2012 <a href="mailto:lincolnbaxter@gmail.com">Lincoln Baxter, III</a>
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
package org.ocpsoft.prettytime;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import java.text.SimpleDateFormat;
import java.util.Date;
import java.util.List;
import java.util.Locale;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;
import org.ocpsoft.prettytime.Duration;
import org.ocpsoft.prettytime.PrettyTime;
import org.ocpsoft.prettytime.TimeUnit;
import org.ocpsoft.prettytime.format.SimpleTimeFormat;

public class PrettyTimeI18n_FA_Test
{
   SimpleDateFormat format = new SimpleDateFormat("MM/dd/yyyy");

   // Stores current locale so that it can be restored
   private Locale locale;

   // Method setUp() is called automatically before every test method
   @Before
   public void setUp() throws Exception
   {
      locale = new Locale("fa");
      Locale.setDefault(locale);
   }

   @Test
   public void testCeilingInterval() throws Exception
   {
      Date then = format.parse("5/20/2012");
      Date ref = format.parse("6/17/2012");
      PrettyTime t = new PrettyTime(ref);
      assertEquals("1 ماه پیش", t.format(then));
   }

   @Test(expected=IllegalArgumentException.class)
   public void testNullDate() throws Exception
   {
      PrettyTime t = new PrettyTime();
      Date date = null;
      assertEquals("چند لحظه دیگر", t.format(date));
   }

   @Test
   public void testRightNow() throws Exception
   {
      PrettyTime t = new PrettyTime();
      assertEquals("چند لحظه دیگر", t.format(new Date()));
   }

   @Test
   public void testRightNowVariance() throws Exception
   {
      PrettyTime t = new PrettyTime(new Date(0));
      assertEquals("چند لحظه دیگر", t.format(new Date(600)));
   }

   @Test
   public void testMinutesFromNow() throws Exception
   {
      PrettyTime t = new PrettyTime(new Date(0));
      assertEquals("12 دقیقه دیگر", t.format(new Date(1000 * 60 * 12)));
   }

   @Test
   public void testHoursFromNow() throws Exception
   {
      PrettyTime t = new PrettyTime(new Date(0));
      assertEquals("3 ساعت دیگر", t.format(new Date(1000 * 60 * 60 * 3)));
   }

   @Test
   public void testDaysFromNow() throws Exception
   {
      PrettyTime t = new PrettyTime(new Date(0));
      assertEquals("3 روز دیگر", t.format(new Date(1000 * 60 * 60 * 24 * 3)));
   }

   @Test
   public void testWeeksFromNow() throws Exception
   {
      PrettyTime t = new PrettyTime(new Date(0));
      assertEquals("3 هفته دیگر", t.format(new Date(1000 * 60 * 60 * 24 * 7 * 3)));
   }

   @Test
   public void testMonthsFromNow() throws Exception
   {
      PrettyTime t = new PrettyTime(new Date(0));
      assertEquals("3 ماه دیگر", t.format(new Date(2629743830L * 3L)));
   }

   @Test
   public void testYearsFromNow() throws Exception
   {
      PrettyTime t = new PrettyTime(new Date(0));
      assertEquals("3 سال دیگر", t.format(new Date(2629743830L * 12L * 3L)));
   }

   @Test
   public void testDecadesFromNow() throws Exception
   {
      PrettyTime t = new PrettyTime(new Date(0));
      assertEquals("3 دهه دیگر", t.format(new Date(315569259747L * 3L)));
   }

   @Test
   public void testCenturiesFromNow() throws Exception
   {
      PrettyTime t = new PrettyTime(new Date(0));
      assertEquals("3 قرن دیگر", t.format(new Date(3155692597470L * 3L)));
   }

   /*
    * Past
    */
   @Test
   public void testMomentsAgo() throws Exception
   {
      PrettyTime t = new PrettyTime(new Date(6000));
      assertEquals("چند لحظه پیش", t.format(new Date(0)));
   }

   @Test
   public void testMinutesAgo() throws Exception
   {
      PrettyTime t = new PrettyTime(new Date(1000 * 60 * 12));
      assertEquals("12 دقیقه پیش", t.format(new Date(0)));
   }

   @Test
   public void testHoursAgo() throws Exception
   {
      PrettyTime t = new PrettyTime(new Date(1000 * 60 * 60 * 3));
      assertEquals("3 ساعت پیش", t.format(new Date(0)));
   }

   @Test
   public void testDaysAgo() throws Exception
   {
      PrettyTime t = new PrettyTime(new Date(1000 * 60 * 60 * 24 * 3));
      assertEquals("3 روز پیش", t.format(new Date(0)));
   }

   @Test
   public void testWeeksAgo() throws Exception
   {
      PrettyTime t = new PrettyTime(new Date(1000 * 60 * 60 * 24 * 7 * 3));
      assertEquals("3 هفته پیش", t.format(new Date(0)));
   }

   @Test
   public void testMonthsAgo() throws Exception
   {
      PrettyTime t = new PrettyTime(new Date(2629743830L * 3L));
      assertEquals("3 ماه پیش", t.format(new Date(0)));
   }

   @Test
   public void testCustomFormat() throws Exception
   {
      PrettyTime t = new PrettyTime(new Date(0));
      TimeUnit unit = new TimeUnit()
      {
         @Override
         public long getMaxQuantity()
         {
            return 0;
         }

         @Override
         public long getMillisPerUnit()
         {
            return 5000;
         }
      };
      t.clearUnits();
      t.registerUnit(unit, new SimpleTimeFormat()
      .setSingularName("tick").setPluralName("ticks")
      .setPattern("%n %u").setRoundingTolerance(20)
      .setFutureSuffix("... RUN!")
      .setFuturePrefix("self destruct in: ").setPastPrefix("self destruct was: ").setPastSuffix(
               " ago..."));

      assertEquals("self destruct in: 5 ticks ... RUN!", t.format(new Date(25000)));
      t.setReference(new Date(25000));
      assertEquals("self destruct was: 5 ticks ago...", t.format(new Date(0)));
   }

   @Test
   public void testYearsAgo() throws Exception
   {
      PrettyTime t = new PrettyTime(new Date(2629743830L * 12L * 3L));
      assertEquals("3 سال پیش", t.format(new Date(0)));
   }

   @Test
   public void testDecadesAgo() throws Exception
   {
      PrettyTime t = new PrettyTime(new Date(315569259747L * 3L));
      assertEquals("3 دهه پیش", t.format(new Date(0)));
   }

   @Test
   public void testCenturiesAgo() throws Exception
   {
      PrettyTime t = new PrettyTime(new Date(3155692597470L * 3L));
      assertEquals("3 قرن پیش", t.format(new Date(0)));
   }

   @Test
   public void testWithinTwoHoursRounding() throws Exception
   {
      PrettyTime t = new PrettyTime();
      assertEquals("2 ساعت پیش", t.format(new Date(new Date().getTime() - 6543990)));
   }

   @Test
   public void testPreciseInTheFuture() throws Exception
   {
      PrettyTime t = new PrettyTime();
      List<Duration> durations = t.calculatePreciseDuration(new Date(new Date().getTime() + 1000
               * (10 * 60 + 5 * 60 * 60)));
      assertTrue(durations.size() >= 2); // might be more because of milliseconds between date capturing and result
      // calculation
      assertEquals(5, durations.get(0).getQuantity());
      assertEquals(10, durations.get(1).getQuantity());
   }

   @Test
   public void testPreciseInThePast() throws Exception
   {
      PrettyTime t = new PrettyTime();
      List<Duration> durations = t.calculatePreciseDuration(new Date(new Date().getTime() - 1000
               * (10 * 60 + 5 * 60 * 60)));
      assertTrue(durations.size() >= 2); // might be more because of milliseconds between date capturing and result
      // calculation
      assertEquals(-5, durations.get(0).getQuantity());
      assertEquals(-10, durations.get(1).getQuantity());
   }

   @Test
   public void testFormattingDurationListInThePast() throws Exception
   {
      PrettyTime t = new PrettyTime(new Date(1000 * 60 * 60 * 24 * 3 + 1000 * 60 * 60 * 15 + 1000 * 60 * 38));
      List<Duration> durations = t.calculatePreciseDuration(new Date(0));
      assertEquals("3 روز 15 ساعت 38 دقیقه پیش", t.format(durations));
   }

   @Test
   public void testFormattingDurationListInTheFuture() throws Exception
   {
      PrettyTime t = new PrettyTime(new Date(0));
      List<Duration> durations = t.calculatePreciseDuration(new Date(1000 * 60 * 60 * 24 * 3 + 1000 * 60 * 60 * 15
               + 1000 * 60 * 38));
      assertEquals("3 روز 15 ساعت 38 دقیقه دیگر", t.format(durations));
   }

   @Test
   public void testSetLocale() throws Exception
   {
      PrettyTime t = new PrettyTime(new Date(315569259747L * 3L));
      assertEquals("3 دهه پیش", t.format(new Date(0)));
   }

   // Method tearDown() is called automatically after every test method
   @After
   public void tearDown() throws Exception
   {
      Locale.setDefault(locale);
   }

}
