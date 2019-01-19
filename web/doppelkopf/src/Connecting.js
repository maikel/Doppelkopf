import React from 'react';
import './Connecting.css';
import { FontAwesomeIcon } from '@fortawesome/react-fontawesome';

function Connecting() {
  return (
    <div id="Connecting">
    <div className="image">
      <div><FontAwesomeIcon className="rotate" icon="sync-alt" /></div>
      <p>Verbindung zum Server wird hergestellt.</p>
    </div>
    </div>
  );
}

export default Connecting;