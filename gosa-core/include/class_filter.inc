<?php
/*
 * This code is part of GOsa (http://www.gosa-project.org)
 * Copyright (C) 2003-2008 GONICUS GmbH
 *
 * ID: $$Id$$
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

class filter {

    var $searches= array();
    var $xmlSearches= array();
    var $attributes= array();
    var $overlays = array();
    var $search;
    var $defaultSearch;
    var $category= "";
    var $objectStorage= array();
    var $base= "";
    var $scope= "";
    var $query;
    var $value= "";
    var $initial= false;
    var $scopeMode= "auto";
    var $converter= null;
    var $pid;

    var $excludeDns = array();

    function filter($filename)
    {
        global $config;

        // Load eventually passed filename
        if (!$this->load($filename)) {
            die("Cannot parse $filename!");
        }

        $this->pid= preg_replace("/[^0-9]/", "", microtime(TRUE)); 
    }


    function load($filename)
    {
        $contents = file_get_contents($filename);
        $xmlData= xml::xml2array($contents, 1);

        if (!isset($xmlData['filterdef'])) {
            return false;
        }

        $xmlData= $xmlData["filterdef"];

        // Load filter
        if (isset($xmlData['search'])) {

            // Array conversion
            if (!isset($xmlData['search'][0])) {
                $searches= array($xmlData['search']);
            } else {
                $searches= $xmlData['search'];
            }

            /* Store available searches */
            foreach ($searches as $search) {

                /* Do multi conversation */ 
                if (!isset($search['query'][0])){
                    $search['query']= array($search['query']);
                }

                /* Store search */
                $this->xmlSearches[$search['tag']]= $search;

            }
        } else {
            return false;
        }

        if(isset($xmlData['overlay'])) {
          if (!isset($xmlData['overlay'][0])) {
            $overlays = array($xmlData['overlay']);
          } else {
            $overlays = $xmlData['overlay'];
          }
          foreach($overlays as $overlay) {
            if (isset($overlay['query'])) {
              array_push($this->overlays, $overlay['query']);
            }
          }
        }

        // Transfer scope
        $this->scopeMode= $xmlData['definition']['scope'];
        if ($this->scopeMode == "auto") {
            $this->scope= "one";
        } else {
            $this->scope= $this->scopeMode;
        }

        // Check if scope was set in session
        if(session::global_is_set("filterScopeMode")){
          $this->scopeMode=session::global_get("filterScopeMode");
        }
        if(session::global_is_set("filterScope")){
          $this->scope=session::global_get("filterScope");
        }
        session::global_set("filterScopeMode",$this->scopeMode);
        session::global_set("filterScope",$this->scope); 

        // Transfer attributes
        $this->attributes= $xmlData['definition']['attribute'];
        if (!is_array($this->attributes)) {
            $this->attributes= array($this->attributes);
        }

        // Transfer initial value
        if (isset($xmlData['definition']['initial']) && $xmlData['definition']['initial'] == "true"){
            $this->initial= true;
        }

        // Transfer category
        if (isset($xmlData['definition']['category'])){
            $this->category= $xmlData['definition']['category'];
        }
        if (!is_array($this->category)) {
            $this->category= array($this->category);
        }

        // Initialize searches and default search mode
        $this->defaultSearch= $xmlData['definition']['default'];
        $this->reloadFilters();
        $this->setSearch($this->defaultSearch);

        return true;  
    }


    function reloadFilters()
    {
        $this->searches= array_merge($this->xmlSearches, userFilter::getFilter($this->category));
        $this->setSearch($this->search);
    }


    function setSearch($method= null)
    {
        $patch= null;

        // Maybe our search method has gone?
        if (!isset($this->searches[$method])) {
            $method= $this->defaultSearch;
        }

        // Try to use it, but bail out if there's no help...
        if (isset($this->searches[$method])) {
            $this->query= $this->searches[$method]['query'];
            $this->search= $method;
        } else {
            die ("Invalid search module!");
        }
    }


    function getTextfield($tag, $value= "", $element= null)
    {
        $size= 31;
        $maxlength= 31;
        $result= "<input class='filter_textfield' placeholder='"._("Search")."...' id='$tag' name='$tag' type='text' size='$size' maxlength='{$maxlength}' value='".$value."'>";
        if ($element && isset($element['autocomplete'])) {
            $frequency= "0.5";
            $characters= "1";
            if (isset($element['autocomplete']['frequency'])) {
                $frequency= $element['autocomplete']['frequency'];
            }
            if (isset($element['autocomplete']['characters'])) {
                $characters= $element['autocomplete']['characters'];
            }
            $result.= "<div id='autocomplete$tag' class='autocomplete'></div>".
                "<script type='text/javascript'>".
                "new Ajax.Autocompleter('$tag', 'autocomplete$tag', 'autocomplete.php', { minChars: $characters, frequency: $frequency });".
                "</script>";

            $this->autocompleters[$tag]= $element;
        }
        return $result;
    }


    function getCurrentBase()
    {
        if (isset($this->search->base) && (string)$this->search->scope != "auto") {
            return false;
        }

        return $this->base;
    }


    function getCurrentScope()
    {
        if (isset($this->search->scope) && (string)$this->search->scope != "auto") {
            return (string)$this->search->scope;
        }

        return $this->scope;
    }


    function setConverter($hook)
    {
        $this->converter= $hook;
    }


    function setObjectStorage($storage)
    {
        $this->objectStorage= $storage;    
    }


    function setBase($base)
    {
        $this->base= $base;
    }


    function setCurrentScope($scope)
    {
        $this->scope= $scope;
        session::global_set("filterScopeMode",$this->scopeMode);
        session::global_set("filterScope",$this->scope); 
    }



    function render()
    {
        $content= "<table class='filter-wrapper' summary='"._("Search filter")."'><tr><td>".$this->renderFilterMenu()."</td><td>";
	$content.= "<div class='search-filter'>".$this->getTextfield('search_filter', set_post($this->value), $this->searches[$this->search])."</div>";
        $content.= "&nbsp;<button class='search-filter' type='submit' title='"._("Search")."'>".image("images/find.png")."</button>";
        $content.= "&nbsp;<button class='search-filter' onClick='$(\"search_filter\").value=\"\";$(\"act\").value= \"filter-default\";$(\"exec_act\").click();' type=\"submit\" title=\""._("Delete filter")."\">".image("images/lists/trash.png")."</button>";
        $content.= "</td></tr></table>";

        // Return meta data
        return ("<input type='hidden' name='FILTER_PID' value='".$this->pid."'>".$content);
    }


    function query()
    {
        global $class_mapping;
        $result= array();

        // Return empty list if initial is not set
        if (!$this->initial) {
            $this->initial= true;
            return $result;
        }
        // Look for overlays - if defined also use those queries for
        // the query loop
        $queries = $this->query;
        foreach($this->overlays as $id => $overlay) {
          $queries[] = $overlay;
        }

        // Go thru all queries and merge results
        foreach ($queries as $query) {
            if (!isset($query['backend']) || !isset($query['filter'])) {
                die("No backend specified in search config.");
            }

            // Is backend available?
            $backend= "filter".$query['backend'];
            if (!isset($class_mapping["$backend"])) {
                die("Invalid backend specified in search config.");
            }

            // Load filter and attributes
            $filter= $query['filter'];

            // Handle converters if present
            if ($this->converter) {
                preg_match('/([^:]+)::(.*)$/', $this->converter, $m);
                $filter = call_user_func(array($m[1], $m[2]), preg_replace('/\$/', $this->prepareFilterValue($this->value), $filter));
            }

            // Do not replace escaped \$ - This is required to be able to search for e.g. windows machines.
            $filter = preg_replace("/\\$/", $this->prepareFilterValue($this->value), $filter);

            $result= array_merge($result, call_user_func(array($backend, 'query'), $this->base, $this->scope, $filter, $this->attributes, $this->category, $this->objectStorage));
        }

        // Filter duplicates
        $seen_dns = array();
        foreach ($result as $id => $entry) {
          $dn = $entry['dn'];
          if(in_array($dn, $seen_dns)) {
            unset($result[$id]);
          }
          $seen_dns[] = $dn;
        }

        // Filter excludes
        foreach ($result as $id => $entry) {
          $dn = $entry['dn'];
          if(in_array($dn, $this->excludeDns)) {
            unset($result[$id]);
          }
        }
        
        return ($result);
    }
    
    function prepareFilterValue($value)
    {
        if (empty($value)) {
            // Empty search string matches everything
            $value = '*';
        }
        elseif ((substr($value, 0, 1) == '*') || (substr($value, -1) == '*')) {
            // Wildcard by user, use as-is
        }
        else {
            $value = "*$value*";
        }
        
        return normalizeLdap($value);
    }


    function update()
    {
        //get scope from session to incorporate changes made in other filter
        $this->scope=session::global_get("filterScope");
        $this->scopeMode=session::global_get("filterScopeMode"); 
        if (isset($_POST['FILTER_PID']) && $_POST['FILTER_PID'] == $this->pid) {
            // Save input field
            if (isset($_POST['search_filter'])) {
                $this->value= get_post('search_filter');
            }

            // Save scope if needed
            if ($this->scopeMode == "auto" && isset($_POST['act']) && $_POST['act'] == "toggle-subtree") {
                $this->scope= ($this->scope == "one")?"sub":"one";
            }

            // Switch filter?
            if (isset($_POST['act'])) {
                foreach ($this->searches as $tag => $cfg) {
                    if ($_POST['act'] == "filter-$tag") {
                        $this->setSearch($tag);
                        break;
                    }
                }
            }
        }
        session::global_set("filterScopeMode",$this->scopeMode);
        session::global_set("filterScope",$this->scope); 

    }


    function getCompletitionList($config, $value="*")
    {
        global $class_mapping;
        $res= array();

        // Load result attributes
        $attributes= $config['autocomplete']['attribute'];
        if (!is_array($attributes)) {
            $attributes= array($attributes);
        }

        // Do the query
        $result= array();

        // Is backend available?
# FIXME
        $queries= $config['query'];
        if (!isset($queries[0])){
            $queries= array($queries);
        }
        foreach ($queries as $query) {
            $backend= "filter".$query['backend'];
            if (!isset($class_mapping["$backend"])) {
                die("Invalid backend specified in search config.");
            }
            $filter= preg_replace("/\\$/", "*".normalizeLdap($value)."*", $query['filter']);

            $result= array_merge($result, call_user_func(array($backend, 'query'), $this->base, $this->scope, $filter, $attributes,
                        $this->category, $this->objectStorage));
        }

        foreach ($result as $entry) {
            foreach ($attributes as $attribute) {
                if (is_array($entry[$attribute])) {
                    for ($i= 0; $i<$entry[$attribute]['count']; $i++) {
                        if (mb_stristr($entry[$attribute][$i], $value)) {
                            $res[]= $entry[$attribute][$i];
                        }
                    }
                } else {
                    $res[]= $entry[$attribute];
                }
            }
        }

        return $res;
    }


    function processAutocomplete()
    {
        global $class_mapping;
        $result= array();

        // Introduce maximum number of entries
        $max= 25;

        if(isset($this->searches[$this->search]['autocomplete'])){
            $result= $this->getCompletitionList($this->searches[$this->search], get_post('search_filter'));
            $result= array_unique($result);
            asort($result);

            echo '<ul>';
            foreach ($result as $entry) {
                echo '<li>'.mark(get_post('search_filter'), $entry).'</li>';
                if ($max-- == 0) {
                    break;
                }
            }

            echo '</ul>';
        }
    }


    function getObjectBase($dn)
    {
        global $config;
        $base= "";

        // Try every object storage
        $storage= $this->objectStorage;
        if (!is_array($storage)){
            $storage= array($storage);
        }
        foreach ($storage as $location) {
            $pattern= "/^[^,]+,".preg_quote($location, '/')."/i";
            $base= preg_replace($pattern, '', $dn);
        }

        /* Set to base, if we're not on a correct subtree */
        if (!isset($config->idepartments[$base])){
            $base= $config->current['BASE'];
        }

        return $base;
    }



    function renderFilterMenu()
    {
        // Load shortcut
        $result= "<ul class='level1' id='filter-root'><li><a href='#'>".image("images/filter.png").image("images/lists/sort-down.png")."</a>";

        // Build ul/li list
        $separator= " style='border-top:1px solid #AAA' ";
        $result.= "<ul class='level2'>";

        // Build in filters
        foreach ($this->xmlSearches as $tag => $config) {
            $label = htmlentities(_($config['label']),ENT_COMPAT,'UTF-8');
            if ($tag == $this->search) {
                $result.= "<li><a href='#'>".image("images/checked.png")."&nbsp;".$label."</a></li>";
            } else {
                $result.= "<li><a href='#' onClick='\$(\"act\").value= \"filter-$tag\";\$(\"exec_act\").click();'>".image("images/empty.png")."&nbsp;".$label."</a></li>";
            }
        }

        // User defined filters
        $first= true;
        foreach (userFilter::getFilter($this->category) as $tag => $config) {
            if ($tag == $this->search) {
                $result.= "<li".($first?$separator:"")."><a href='#'>".image("images/checked.png")."&nbsp;"._($config['description'])."</a></li>";
            } else {
                $result.= "<li".($first?$separator:"")."><a href='#' onClick='\$(\"act\").value= \"filter-$tag\";\$(\"exec_act\").click();'>".image("images/empty.png")."&nbsp;"._($config['description'])."</a></li>";
            }

            $first= false;
        }

        // Render scope if set to auto
        if ($this->scopeMode == "auto") {
            $result.= "<li$separator><a href='#' onClick='\$(\"act\").value= \"toggle-subtree\";\$(\"exec_act\").click();'>".($this->scope=="one"?image("images/empty.png"):image("images/checked.png"))."&nbsp;"._("Search in subtrees")."</a></li>";
        }


        // Edit filter menu
        $result.= "<li$separator><a href='#' onClick='\$(\"act\").value= \"config-filter\";\$(\"exec_act\").click();'>".image("images/configure.png")."&nbsp;"._("Edit filters")."...</a></li>";

        $result.= "</ul>";

        $script= '<script type="text/javascript" language="JavaScript">var menu2; menu2= new Menu("filter-root", "menu2", configMenu)</script>';

        return "<div id='filtermenu'>".$result."</li></ul></div>$script";
    }


    function getFixedFilters()
    {
        return array_keys($this->searches);
    }

    function setExcludeDns($dns) {
        $this->excludeDns = $dns;
    }

}


// vim:tabstop=2:expandtab:shiftwidth=2:filetype=php:syntax:ruler:
?>
