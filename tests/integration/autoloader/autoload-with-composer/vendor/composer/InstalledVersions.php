<?php
/*
 * Copyright 2020 New Relic Corporation. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 */

/*DESCRIPTION
This file is the core of real Composer's runtime API, and agent verifies its presence to determine if Composer is used.
It contains \Composer\InstalledVersions class with methods used by the agent to get the installed packages and their versions.
*/

namespace Composer;
class InstalledVersions
{
    // This Composer's runtime API method is used by the agent to get the installed version of a package:
    public static function getVersion(string $packageName)
    {
        $installed = self::getAllRawData();
        return $installed[0]['versions'][$packageName]['version'];
    }

    // This Composer's runtime API method is used by the agent to get the list of installed packages:
    public static function getInstalledPackages()
    {
        // Return the package names
        $installed = self::getAllRawData();
        return array_keys($installed[0]['versions']);
    }

    // This Composer's runtime API method is used by the agent to get the list of installed packages:
    public static function getAllRawData()
    {
        $installed = require __DIR__ . '/installed.php';
        // This mock only returns a single dataset; in real life, there could be more
        return array($installed);
    }

    // This Composer's runtime API method is used by the agent to get the root package:
    public static function getRootPackage()
    {
        $installed = self::getAllRawData();
        // This mock only returns a single dataset; in real life, there could be more
        return $installed[0]['root'];
    }

    // Mock of 'composer show' used by integration tests to generate list of packages:
    public static function show() {
        $installed = self::getAllRawData();
        foreach ($installed[0]['versions'] as $package => $info) {
            $version = ltrim($info['pretty_version'], 'v');
            echo "$package => $version\n";
        }
    }
}
