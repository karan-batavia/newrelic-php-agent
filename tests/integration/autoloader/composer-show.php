<?php

/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
Mock of 'composer show' used by integration tests to generate list of packages.
*/

$installedVersions = "autoload-with-composer/vendor/composer/InstalledVersions.php";

if ($argc > 1) {
    $installedVersions = $argv[1];
}

include $installedVersions;
if ($argc > 2) {
    $package = $argv[2];
    $version = Composer\InstalledVersions::getVersion($package);
    echo "$package => $version\n";
} else {
    Composer\InstalledVersions::show();
}
