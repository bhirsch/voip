<?php

// $Id$

/**
 * @file
 * Implementation of core functionality associated with voip scripts
 */


/*
 * Class definitions
 */

        
/*
 * Class that keeps track of both the commands as well as the current status of 
 * a Voip Script
 */
class VoipScript {
  
  // Script variables

  /**
   * Script name
   */
  private $name;

  /**
   * Array of script commands
   */
  private $commands; 

  /**
   * Index of the next command to be executed
   */
  private $index; // index of the next command to be executed

  /**
   * Script variables
   */
  private $variables;

  /**
   * Stack of scripts being executed via gosub commands.
   *
   * Each element of the array stores the current status of a script,
   * including name, commands, and command index
   */
  private $stack;

  /**
   * Flag indicating script execution error
   */
  protected $is_error;

  /**
   * Script error message
   */
  protected $error_message;

  /*
   * Constants
   */

  /* 
   * Possible status of a call
   */

  const ERROR='processing_error';
  const NOT_CALLED='not_called'; // the call has yet to be called
  const DIALING='dialing'; // the call is being connected/requested
  const NO_ANSWER='no_answer';
  const BUSY='busy';
  const INVALID='invalid_number';
  const IN_PROGRESS='in_progress'; // the call is in progress
  const ANSWERED_MACHINE='answered_by_machine';
  const ANSWERED_HUMAN='answered_by_human';
  const MACHINE_HANGUP='machine_hangup'; // call hangup due to machine pickup
  const COMPLETED='completed';


  /**
   * Indicator of no keypad input received
   */
  const NO_INPUT='no_input';


  /**
   * VoipScript constructor
   *
   * @param $name 
   *   A string containing the name of the new script
   * @param $variables 
   *   An array with the initial value of the script variables
   *
   * @return
   *  The new script created
   */
  function __construct($name, $variables=array()) {
    $this->name = $name;
    $this->commands = array();
    $this->index = 0;
    $this->variables = $variables;
    $this->stack = array();
    $this->is_error = FALSE;
    $this->error_message = NULL;
  }

  /**
   * Returns a new instance of an already-existing script
   *
   * @param $script_name
   *   String containing the name of the script to be loaded
   * @param array $params
   *   Array with the value pairs to be passed to hook_voipscript_load_script()
   *
   * @return
   *   The new script instance, or NULL in case of invalid script name
   */
  public static function loadScript($script_name, $params = NULL){
    $rc = NULL;
    $script_tmp = new VoipScript($script_name);
    $script_tmp->runLoad($script_name, $params);
    if(!$script_tmp->isError()) {
      $rc = $script_tmp;
    }
    return $rc;
  }
        

  /*
   * Public methods
   */

  /**
   * Returns the script name
   */
  function getName() {
    return $this->name;
  }

  /**
   * Returns the value of the specified script variable
   */
  function getVar($name) {
    return $this->variables[$name];
  }

  /**
   * Sets the value of the specified script variable
   */
  function setVar($name, $value) {
    $this->variables[$name] = $value;
  }

  /**
   * Unsets the specified script variable
   */
  function unsetVar($name) {
    unset($this->variables[$name]);
  }


  /*
   * Methods used during script execution
   */

  /**
   * Returns the next script command to be executed
   */
  function getNextCommand() {
    $cmd = $this->commands[$this->index];
    $this->index++;
    return $cmd;
  }

  /**
   * Resets the script index
   */
  function resetIndex() {
    $this->index = 0;
  }

  /**
   * Evaluates the given string based on the current value of script variables.
   *
   * @param $string
   *   The string to be evaluated
   *
   * @return 
   *   The result of the evaluation
   *
   * @see _voipscript_expand_variables() for the actual syntax allowed in the 
   *   string to be evaluated
   */
  function evalString($string) {
    $rc = _voipscript_expand_variables($string, $this->variables);
    return $rc;
  }

  /**
   * Returns TRUE in case a script processing error has been detected
   */
  function isError(){
    return $this->is_error;
  }

  /**
   * Retuns the message associated with a script error.
   */
  function getErrorMessage() {
    return $this->error_message;
  }

  /*
   * Supported script commands
   */

  /**
   * Adds a "dial" command to the script.
   *
   * When executed, the 'dial' command connects the current caller to another
   * phone. If the called party picks up, the two parties are connected and
   * can communicate until one hangs up. If the called party does not pick up,
   * or if a busy signal is received, or the number doesn't exist, the dial
   * command will finish.
   * TODO: Upon completion, the 'dial' command might set the following {@link variables}: (add @uses in here?)
   * TODO: DON'T FORGET to move the status constants into VoipScript...
   *
   * @param $number
   *   String with a valid number to be dialed
   * @param $timeout
   *   Integer with the number of seconds to wait for the called party to
   *   answer the call
   * @param $hangup_on_star
   *   If TRUE, lets the calling party hang up on the called party by pressing
   *   the '*' key on his phone. This feature allows the calling party to hang
   *   up on the called party without having to hang up the phone and end the
   *   session. When the caller presses '*' Voip Drupal will hang up on the
   *   called party and will move on to execute the next command in the script.
   * @param $time_limit
   *   Integer with the maximum duration of the call in seconds
   * @param $caller_id
   *   String with the caller id to be used for the call. By default, this is
   *   the caller id defined in the 'default call configuration' settings
   */
  function addDial($number, $timeout=30, $hangup_on_star=false, $time_limit=3600, $caller_id=''){
    $params['number'] = $number;
    $params['timeout'] = $timeout;
    $params['hangup_on_star'] = $hangup_on_star;
    $params['time_limit'] = $time_limit;
    $params['caller_id'] = $caller_id;
    $this->commands[] = new VoipCommand('VoipCmdDial', $params);
  }

  /*
   * Adds a 'get input' command to the script. 
   *
   * When executed, the 'get input' command reads user input from the phone
   * keypad. The system stops reading the input whenever there is a
   * timeout, the user presses a predefined key, or the users presses the
   * maximum number of digits expected.
   *
   * @param $text 
   *   String with the prompt to be read asking the user to type something
   * @param $url
   *   String with the url of an mp3 or wav file to be played to the user
   *   asking her to input something. This parameter is only used in case
   *   the parameter $text is not provided
   * @param $timeout
   *   Integer specifying the maximum number of seconds to wait in between
   *   digits
   * @param $end_key
   *   Key used to end the data input. The allowed values are the numbers 0-9,
   *   # , * and the empty string.
   * @param $num_digits
   *   Integer with the number of digits that are expected.
   *
   * TODO: out: variable input_digits set to input or to VoipScrip::NO_INPUT
   */
  function addGetInput($text='', $url='', $timeout=5, $end_key='#', $num_digits=NULL) {
    $params['text'] = $text;
    $params['url'] = $url;
    $params['timeout'] = $timeout;
    $params['end_key'] = $end_key;
    $params['num_digits'] = $num_digits;
    $this->commands[] = new VoipCommand('VoipCmdGetInput', $params);
  }

  /**
   * Adds a "gosub" command to the script.
   *
   * When executed, the 'gosub' command saves the state of the current script
   * into the local stack and loads the commands and variables of specified
   * script.
   * 
   * @param $script_name
   *   The name of the script to be loaded
   *
   * @see VoipScript::addReturn()
   */
  function addGosub($script_name) {
    $params['script_name'] = $script_name;
    $this->commands[] = new VoipCommand('VoipCmdGosub', $params);
  }

  /** 
   * Adds a "go to" command to the script.
   * 
   * When ran, the 'go to' command updates the index of the script and skips
   * the script execution to the command that follows the specified label.
   *
   * @param $label_name
   *   String with the name of the label to go to.
   *
   * @see VoipScript::addLabel()
   * @see VoipScript::addGotoIf()
   */ 
  function addGoto($label_name) {
    $params['label_name'] = $label_name;
    $this->commands[] = new VoipCommand('VoipCmdGoto', $params);
  }

  /**
   * Adds a 'goto if' command to the script.
   *
   * This command is similar to VoipScript:addGoto(). However, it only goes to
   * the specified label in case the specified condition evaluates to TRUE.
   *
   * @param $label_name
   *   String with the name of the label to go to.
   * @param $condition
   *   If TRUE, skips script execution to the specified label.
   *
   * @see VoipScript::addLabel()
   * @see VoipScript::addGoto()
   */
  function addGotoIf($label_name, $condition) {
    $params['label_name'] = $label_name;
    $params['condition'] = $condition;
    $this->commands[] = new VoipCommand('VoipCmdGotoIf', $params);
  }

  /**
   * Adds a 'hangup' command to the script.
   *
   * When executed, the 'hangup' command ends the call.
   */
  function addHangup() {
    $this->commands[] = new VoipCommand('VoipCmdHangup');
  }

  function addJoinConference($room, $muted=FALSE, $beep=FALSE, $start_on_enter=TRUE, $end_on_exit=FALSE, $wait_url='', $hangup_on_star=false, $time_limit=3600) {
    $params['room'] = $room;
    $params['muted'] = $muted;
    $params['beep'] = $beep;
    $params['start_on_enter'] = $start_on_enter;
    $params['end_on_exit'] = $end_on_exit;
    $params['wait_url'] = $wait_url;
    $params['hangup_on_star'] = $hangup_on_star;
    $params['time_limit'] = $time_limit;
    $this->commands[] = new VoipCommand('VoipCmdJoinConference', $params);
  }

  function addLabel($label_name) {
    $params['label_name'] = $label_name;
    $this->commands[] = new VoipCommand('VoipCmdLabel', $params);
  }

  function addLoad($script_name) {
    $params['script_name'] = $script_name;
    $this->commands[] = new VoipCommand('VoipCmdLoad', $params);
  }

  function addPlay($url, $loop=1) {
    $params['url'] = $url;
    $params['loop'] = $loop;
    $this->commands[] = new VoipCommand('VoipCmdPlay', $params);
  }

  function addRecord($timeout=5, $end_key='#', $max_length=3600) {
    $params['timeout'] = $timeout;
    $params['end_key'] = $end_key;
    $params['max_length'] = $max_length;
    $this->commands[] = new VoipCommand('VoipCmdRecord', $params);
  }

  function addReturn() {
    $this->commands[] = new VoipCommand('VoipCmdReturn');
  }

  function addSay($text, $loop=1, $voice=NULL, $language=NULL) {
    $params['text'] = $text;
    $params['loop'] = $loop;
    $params['voice'] = $voice;
    $params['language'] = $language;
    $this->commands[] = new VoipCommand('VoipCmdSay', $params);
  }

  function addSet($var_name, $var_value) {
    $params['var_name'] = $var_name;
    $params['var_value'] = $var_value;
    $this->commands[] = new VoipCommand('VoipCmdSet', $params);
  }

  function addUnset($var_name) {
    $params['var_name'] = $var_name;
    $this->commands[] = new VoipCommand('VoipCmdUnset');
  }


  /*
   * Execution of basic script commands that are not dependent on the voip server
   */

  function runGosub($script_name) {
    $script_state = array('name' => $this->name,
                          'commands' => $this->commands,
                          'index' => $this->index);
    $this->stack[] = $script_state;
    $rc =  $this->runLoad($script_name);
    return $rc;
  }

  function runGoto($label_name) {
    $rc = TRUE;
    $found = FALSE;
    $i = 0;
    while(!$found) {
      $cmd = $this->commands[$i];
      if(!$cmd) {
        // label nor found
        $this->is_error = TRUE;
        $script_name = $this->name;
        $this->error_message = "Label '$label_name' not found in script '$script_name': " . print_r($this, true);
        $rc = FALSE;
        $found = TRUE;
      }
      else {
        if($cmd->getId() == 'VoipCmdLabel') {
          $string_tmp = $cmd->getParam('label_name');
          $label_name_tmp = $this->evalString($string_tmp);
          if(strcasecmp($label_name_tmp, $label_name) == 0) {
            $this->index = $i;
            $found = TRUE;
          }
        }
        // move to the next command in the script
        $i++;
      }
    }
    return $rc;
  }

  /**
   * @param $script_name
   *  string, name of script to be loaded
   * 
   * @param $params
   *  array, optional parameters 
   *  to be passed to implementations of hook_voipscript_load_script
   */
  function runLoad($script_name, $params = NULL) {
    $rc = TRUE;
    $found = FALSE;

    // Invoke hook_voipscript_load_script() giving modules the 
    // opportunity to return a custom script. 
    foreach (module_implements('voipscript_load_script') as $module) {
      $function = $module .'_'. 'voipscript_load_script';
      $new_script = call_user_func($function, $script_name, $params);
      if(isset($new_script)) {
        $found = TRUE;
        // loads new name and commands
        $this->name = $new_script->getName();
        $this->commands = $new_script->getCommands();
        $this->index = 0;
        // override variables with the same name and preserve the others
        $this->variables = array_merge($this->variables, $new_script->getVariables());
        // preserve the stack as is
          // $this->stack = array();
        $this->is_error = FALSE;
        // scripts must have unique names. exit the foreach
        break;
      }
    }

    if(!$found) {
      // no script found with the given name
      $rc = FALSE;
      $this->is_error = TRUE;
      $this->error_message = "Script '$script_name' not found for script '" . $this->name . "': " . print_r($this->commands, TRUE);
    }

    return $rc;
  } 

  function runReturn() {
    $rc = TRUE;
    $script_state = array_pop($this->stack);
    if ($script_state) {
      $this->name = $script_state['name'];
      $this->commands = $script_state['commands'];
      // Note: index is already pointing to the next command to be executed
      $this->index = $script_state['index'];
    }
    else {
      // invalid return command
      $this->is_error = TRUE;
      $this->error_message = "Invalid return command for script '" . $this->name . "': " . print_r($this, true);
    }
    return $rc;
  }


  /**
   * Protected methods
   */

  protected function getCommands() {
    return $this->commands;
  }

  protected function getVariables() {
    return $this->variables;
  }

  // the following function is provided in case there is a need to
  // extend the script API to include additional commands
  protected function addCommand($command) {
    $this->commands[] = $command;
  }

}


/**
 * Implementation of command-related classes
 */

class VoipCommand {

  private $id; // command id
  private $params; // command parameters

  function __construct($id, $params=array()) {
    $this->id = $id;
    $this->params = $params;
  }

  function getId() {
    return $this->id;
  }

  function getParam($param_name, $default=NULL) {
    return $this->params[$param_name]?$this->params[$param_name]:$default;
  }
}
    


/**
 * Internal functions
 */

/**
 * The following function recursively replaces predefined variables from
 * inside the given string.  It was based on code made available at 
 * http://stackoverflow.com/questions/3241070/php-recursive-variable-replacement
 *
 * Notes:
 * - Strings that start with '^' are treated as expressions to be evaluated
 * - Placeholders for script variables start with '%'
 * - This function is smart enough to process variables referenced by
 *   other variables
 * - There is no check against circular inclusion, which might simply lead
 *   to an infinite loop. (Example: $vars['s'] = '%s'; ..) 
 * - When defining expressions that include string variables, make sure the
 *   variable placeholder is enclosed in " or '.  For instance,
 *     Correct form:   "^print_r('The content is ' . '%msg');"
 *     Incorrect form: "^print_r('The content is ' . %msg);"
 *   The incorrect form might produce a parser error if the variable msg
 *   contains characters such as a space, math sign, etc... It might also
 *   produce undesirable results if the variable starts with 0.
 */
function _voipscript_expand_variables($str, $vars) {
  $eval = substr($str, 0, 1) == '^';
  $regex = '/\%(\w+)/e';
  $replacement = "_voipscript_replace_variable(\$1, \$vars, \$eval);" ;
  $res = preg_replace($regex, $replacement ,$str);
  if($eval) {
    ob_start();
    $expr = substr($res, 1);
    if(eval('$res = ' . $expr . ';')===false) {
      ob_end_clean();
//      die('Invalid PHP-Expression: '.$expr);
        watchdog('voipscript', 'Invalid PHP expression: @expr', array('@expr' => $expr), WATCHDOG_ERROR);
    }
    ob_end_clean();
  }
  return $res;
}

function _voipscript_replace_variable($var_name, $vars, $eval) {
  if(isset($vars[$var_name])) {
    $expanded = _voipscript_expand_variables($vars[$var_name], $vars);
    if($eval) {
      // Special handling since $str is going to be evaluated ..
      if(!is_numeric($expanded) || (substr($expanded . '', 0, 1)==='0'
          && strpos($expanded . '', '.')===false)) {
        $expanded = "'$expanded'";
      }
    }
    return $expanded;
  } else {
    // Variable does not exist in $vars array
    if($eval) {
      return 'null';
    }
//  return "$var_name";
    return '';
  }
}

