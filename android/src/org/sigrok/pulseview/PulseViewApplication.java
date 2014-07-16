/*
 * This file is part of the PulseView project.
 *
 * Copyright (C) 2014 Marcus Comstedt <marcus@mc.pp.se>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

package org.sigrok.pulseview;

import org.qtproject.qt5.android.bindings.QtApplication;
import org.sigrok.androidutils.Environment;
import org.sigrok.androidutils.UsbHelper;

import java.io.File;
import java.io.IOException;

public class PulseViewApplication extends QtApplication
{
    @Override
    public void onCreate()
    {
	Environment.initEnvironment(getApplicationInfo().sourceDir);
	UsbHelper.setContext(getApplicationContext());
	super.onCreate();
    }
}

