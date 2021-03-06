# Copyright(C) 2014 Toshihisa Tashiro
# Copyright(C) 2016 Kouhei Sutou <kou@clear-code.com>
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Lesser General Public
# License as published by the Free Software Foundation; either
# version 2.1 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this library; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

--disable_query_log
let $version_compile_os_osx=`SELECT IF(@@version_compile_os like 'osx%', 1, 0);`;
if ($version_compile_os_osx) {
  let $version_compile_os_osx_10_8_or_later=
    `SELECT IF(@@version_compile_os = 'osx10.6', 0, 1);`;
  if ($version_compile_os_osx_10_8_or_later) {
    let $version_compile_os_osx_10_8_or_later=
      `SELECT IF(@@version_compile_os = 'osx10.7', 0, 1);`;
  }
}
if (!$version_comiple_os_osx) {
  let $version_compile_os_osx_10_8_or_later=0;
}
--enable_query_log
