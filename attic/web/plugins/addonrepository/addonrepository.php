<?php
/**
 * @file addonrepository.php
 *
 * @section License
 * GPL: http://www.gnu.org/licenses/gpl.html
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 *
 * @author Copyright &copy; 2013 Daniel Swanson <danij@dengine.net>
 */

includeGuard('AddonRepositoryPlugin');

require_once('baseaddon.class.php');
require_once('addonsparser.class.php');

class AddonRepositoryPlugin extends Plugin implements Actioner, RequestInterpreter
{
    public static $name = 'addonrepository';

    public static $baseRequestName = 'addons';

    private $_displayOptions = 0;

    // Symbolic game mode names:
    public static $doomGameModes = array(
        'doom1', 'doom1-ultimate', 'doom1-share', 'doom2', 'doom2-plut', 'doom2-tnt'
    );

    public static $hereticGameModes = array(
        'heretic', 'heretic-share', 'heretic-ext'
    );

    public static $hexenGameModes = array(
        'hexen', 'hexen-dk', 'hexen-demo'
    );

    private $addons = NULL;

    public function __construct() {}

    public function title()
    {
        return 'Add-ons';
    }

    /**
     * Implements RequestInterpreter
     */
    public function InterpretRequest($request)
    {
        $url = urldecode($request->url()->path());

        // @kludge skip over the first '/' in the home URL.
        $url = substr($url, 1);

        // Remove any trailing file extension, such as 'something.html'
        if(strrpos($url, '.'))
            $url = substr($url, 0, strrpos($url, '.'));

        // Tokenize the request URL so we can begin to determine what to do.
        $tokens = explode('/', $url);

        if(count($tokens) && !strcasecmp(self::$baseRequestName, $tokens[0]))
        {
            FrontController::fc()->enqueueAction($this, NULL);
            return true; // Eat the request.
        }

        return false; // Not for us.
    }

    private function outputAddonListElement(&$addon)
    {
        if(!$addon instanceof Addon)
            throw new Exception('Invalid addon argument, Addon expected');

        $addonFullTitle = $addon->title();
        $addonVersion = $addon->version();
        if(strlen($addonVersion) > 0)
            $addonFullTitle .= ' '. $addonVersion;

?><tr><td><?php

        if($addon->hasDownloadUri())
        {
?><a href="<?php echo $addon->downloadUri(); ?>" title="Download &#39;<?php echo htmlspecialchars($addonFullTitle); ?>&#39;" rel="nofollow"><?php echo htmlspecialchars($addonFullTitle); ?></a><?php
        }
        else if($addon->hasHomepageUri())
        {
?><a href="<?php echo $addon->homepageUri(); ?>" title="Visit homepage for &#39;<?php echo htmlspecialchars($addonFullTitle); ?>&#39;" rel="nofollow"><?php echo htmlspecialchars($addonFullTitle); ?></a><?php
        }
        else
        {
            echo htmlspecialchars($addonFullTitle);
        }

?></td>
<td><?php if($addon->hasDescription()) echo htmlspecialchars($addon->description()); ?></td>
<td><?php if($addon->hasNotes()) echo htmlspecialchars($addon->notes()); ?></td></tr><?
    }

    /**
     * Output an HTML list of addons to the output stream.
     *
     * @param filter_gameModes  (Array) Game modes to filter the addon list by.
     * @param filter_featured  (Mixed) @c < 0 no filter
     *                                    = 0 not featured
     *                                    = 1 featured
     */
    private function outputAddonList($filter_gameModes, $filter_featured=-1)
    {
        // Sanitize filter arguments.
        $filter_featured = intval($filter_featured);
        if($filter_featured < 0) $filter_featured = -1; // Any.
        else                     $filter_featured = $filter_featured? 1 : 0;

        // Output the table.
?><table class="directory">
<thead>
<tr>
<th><label title="Package Name">Name</label></th>
<th style="width:50%"><label title="Package Description">Description</label></th>
<th><label title="Package Notes">Notes</label></th></tr>
</thead><?php

        foreach($this->addons as &$addon)
        {
            if($filter_featured != -1 && (boolean)$filter_featured != $addon->hasFeatured()) continue;
            if(!$addon->supportsGameMode($filter_gameModes)) continue;

            $this->outputAddonListElement($addon);
        }

?></table><?php

    }

    private function outputFeaturedAddons()
    {
?><div class="block"><div class="addons_list"><?php

        foreach($this->addons as &$addon)
        {
            if(!$addon->hasFeatured()) continue;

            echo $addon->genDownloadBadge();
        }

?></div></div><?php
    }

    public static function packageSorter($packA, $packB)
    {
        return strcmp($packA->title(), $packB->title());
    }

    /**
     * Implements Actioner.
     */
    public function execute($args=NULL)
    {
        $fc = &FrontController::fc();

        // Build the add-ons collection.
        $addonListXml = file_get_contents(nativePath("plugins/addonrepository/addons.xml"));

        $this->addons = array();
        AddonsParser::parse($addonListXml, $this->addons);

        // Sort the collection.
        uasort($this->addons, array('self', 'packageSorter'));

        // Output the page.
        $fc->outputHeader($this->title(), 'addons');
        $fc->beginPage($this->title(), 'addons');

?><div id="contentbox" class="addons"><?php

        includeHTML('overview', self::$name);

        $this->outputFeaturedAddons();

?><div class="block"><article><h1>DOOM</h1>
<p>The following add-ons are for use with <a href="/doom" title="Tell me more about DOOM">DOOM</a> or a variant of it such as <strong>DOOM2</strong> and <strong>Final Doom: The Plutonia Experiment</strong>.</p>
<?php

        $this->outputAddonList(self::$doomGameModes);

?></article></div><?php

?><div class="block"><article><h1>Heretic</h1>
<p>The following add-ons are for use with <a href="/heretic" title="Tell me more about Heretic">Heretic</a> and the <strong>Shadow of the Serpent Riders</strong> expansion pack.</p>
<?php

        $this->outputAddonList(self::$hereticGameModes);

?></article></div><?php

?><div class="block"><article><h1>Hexen</h1>
<p>The following add-ons are for use with <a href="/hexen" title="Tell me more about Hexen">Hexen</a> and the <strong>Deathkings of the Dark Citadel</strong> expansion pack.</p>
<?php

        $this->outputAddonList(self::$hexenGameModes);

?></article></div><?php
?></div><?php

        $fc->endPage();
    }
}
