import React, { Component } from 'react';
import './App.css';
import Table from './Table.js';
import Lobby from './Lobby.js';
import Connecting from './Connecting.js';
import { library } from '@fortawesome/fontawesome-svg-core'
import { faIgloo, fas } from '@fortawesome/free-solid-svg-icons'

library.add(faIgloo)
library.add(fas)

class App extends Component {
  constructor(props) {
    super(props);
    this.state = { 
      "viewport": "connecting",
      "joined_table": null,
      "tables": [],
      "clients": []
    };
    this.startWebSocket();
    this.startWebSocket = this.startWebSocket.bind(this);
  }

  startWebSocket() {
    this.ws = new WebSocket("ws://192.168.178.84:8000");
    this.ws.onopen = this.onOpen.bind(this);
    this.ws.onmessage = this.onRead.bind(this);
    this.ws.onerror = this.onError.bind(this);
    this.ws.onclose = this.onClose.bind(this);
  }

  onOpen() {
    console.log("Opened Websocket to localhost.");
    this.setState({viewport: "lobby"});
  } 

  onRead(event) {
    let state = JSON.parse(event.data);
    if (state.hasOwnProperty('error')) {
      console.log(">> ", state);
    }  else {
      console.log(">> ", state);
      if (state.hasOwnProperty('joined_table') && state.joined_table !== null) {
        state.viewport = "table";
      } else if (state.hasOwnProperty('joined_table') && state.joined_table === null) {
        state.viewport = "lobby";
      }
      this.setState(state);
    }
  }

  leaveTable() {
    this.setState({joined_table: null, viewport: "lobby"});
    this.send({command: "tables"});
  }

  onError(event) {
    console.log("Error in websocket: ", event);
  }

  onClose(event) {
    console.log("Connection closed: " + event);
    this.setState({viewport: "connecting"});
    setTimeout(this.startWebSocket, 5000);
  }

  send(obj) {
    let msg = JSON.stringify(obj);
    this.ws.send(msg);
    console.log("<< ", obj);
  }

  viewport() {
    switch (this.state.viewport) {
      case "table":
        return (<Table leaveTable={this.leaveTable.bind(this)}
                       send={this.send.bind(this)} 
                       joinedTable={this.state.joined_table} />);   
      case "lobby":
        return (<Lobby send={this.send.bind(this)} 
                       tables={this.state.tables} />);
      default:
        return (<Connecting />); 
    }
  }

  render() {
    return (
      <div className="App">
        {this.viewport()}
      </div>
    );
  }
}

export default App;