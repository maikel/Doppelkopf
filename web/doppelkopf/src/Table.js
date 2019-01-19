import React, { Component } from 'react';
import './Table.css';
import Hand from './Hand.js';
import Trick from './Trick.js';

class DeclareContracts extends Component {
  constructor(props) {
    super(props);
    this.sendChoice = this.sendChoice.bind(this);
  }

  thisplayer() {
    return this.props.state.player_id;
  }

  getPlayer(shift) {
    let this_id = this.thisplayer();
    let shifted = (this_id + 4 + shift) % 4;
    return this.props.state.players[shifted].name;
  }

  sendChoice(event) {
    this.props.send({
      command: "choose",
      declared_contract: {
        player: this.thisplayer(),
        health: event.target.name
      }
    });
  }

  render() {
    return (
      <div className="Contracts">
        <p>{this.getPlayer(-1)}</p>
        <p>{this.getPlayer(0)}</p>
        <p>{this.getPlayer(+1)}</p>
        <div><button name="healthy" onClick={this.sendChoice}>Gesund</button></div>
        <div><button name="reservation" onClick={this.sendChoice}>Vorbehalt</button></div>
        <div><button onClick={this.props.leaveTable}>Zurück</button></div>
        <Hand cards={this.props.state.hand} />
      </div>
    );
  }
}

class SpecializeContracts extends Component {
  constructor(props) {
    super(props);
    this.sendChoice = this.sendChoice.bind(this);
  }

  thisplayer() {
    return this.props.state.player_id;
  }

  getPlayer(shift) {
    let this_id = this.thisplayer();
    let shifted = (this_id + 4 + shift) % 4;
    return this.props.state.players[shifted].name;
  }

  sendChoice(event) {
    this.props.send({
      command: "choose",
      specialized_contract: {
        player: this.thisplayer(),
        rules: event.target.name
      }
    });
  }

  render() {
    return (
      <div className="Contracts">
        <p>{this.getPlayer(-1)}</p>
        <p>{this.getPlayer(0)}</p>
        <p>{this.getPlayer(+1)}</p>
        <div><button name="normal" onClick={this.sendChoice}>Normalspiel</button></div>
        <div><button name="marriage" onClick={this.sendChoice}>Hochzeit</button></div>
        <div><button name="jack" onClick={this.sendChoice}>Bubensolo</button></div>
        <div><button name="queen" onClick={this.sendChoice}>Damensolo</button></div>
        <div><button name="diamonds" onClick={this.sendChoice}>Karosolo</button></div>
        <div><button name="hearts" onClick={this.sendChoice}>Herzsolo</button></div>
        <div><button name="spades" onClick={this.sendChoice}>Piksolo</button></div>
        <div><button name="clubs" onClick={this.sendChoice}>Kreuzsolo</button></div>
        <div><button onClick={this.props.leaveTable}>Zurück</button></div>
        <Hand cards={this.props.state.hand} />
      </div>
    );
  }
}

class Table extends Component {
  constructor(props) {
    super(props);
    this.leaveTable = this.leaveTable.bind(this);
    this.takeSeat = this.takeSeat.bind(this);
  }

  leaveTable() {
    this.props.send({ command: "leave_table" });
    this.props.leaveTable();
  }

  takeSeat(event) {
    this.props.send({ command: "take_seat", seat: parseInt(event.target.name) });
  }

  showPlayer(player_id) {
    if (this.props.joinedTable.players[player_id] !== null) {
      return this.props.joinedTable.players[player_id].name
        + (this.props.joinedTable.player_id === player_id ? " (Du)" : "");
    }
    return "Nicht belegt";
  }

  getTrick() {
    let game = this.props.joinedTable.game;
    if (game.hasOwnProperty("trick") && game.trick !== null) {
      return game.trick;
    }
    return [];
  }

  getHand() {
    let game = this.props.joinedTable.game;
    if (game.hasOwnProperty("hand") && game.hand !== null) {
      return game.hand;
    }
    return [];
  }

  render() {
    if (this.props.joinedTable.game !== null) {
      switch (this.props.joinedTable.game.state_type) {
        case 'declare_contracts':
          let dstate = {
            player_id: this.props.joinedTable.player_id,
            players: this.props.joinedTable.players,
            hand: this.getHand()
          };
          return (
            <DeclareContracts state={dstate} send={this.props.send} leaveTable={this.leaveTable} />
          )
        case 'specialized_contracts':
          let sstate = {
            player_id: this.props.joinedTable.player_id,
            players: this.props.joinedTable.players,
            hand: this.getHand()
          };
          return (
            <SpecializeContracts state={sstate} send={this.props.send} leaveTable={this.leaveTable} />
          );
        case 'running':
          return (
            <div className="Table">
              <Trick cards={this.getTrick()} />
              <Hand cards={this.getHand()} />
            </div>
          );
        case 'scoring':
        default:
          return (
            <div>
              Ein Fehler ist aufgetreten.
            <button onClick={this.leaveTable}>Zurück</button>
            </div>
          )
      }
    } else {
      if (!this.props.joinedTable.hasOwnProperty("player_id")
        || this.props.joinedTable.player_id === null) {
        return (
          <div id="Seating">
            <p>Das Spiel hat noch nicht angefangen.</p>
            <div><button name="0" onClick={this.takeSeat}>Wähle Sitz 1</button></div>
            <div><button name="1" onClick={this.takeSeat}>Wähle Sitz 2</button></div>
            <div><button name="2" onClick={this.takeSeat}>Wähle Sitz 3</button></div>
            <div><button name="3" onClick={this.takeSeat}>Wähle Sitz 4</button></div>
            <div><button onClick={this.leaveTable}>Zurück</button></div>
          </div>
        );
      } else {
        return (
          <div>
            <p>Du bist Spieler #{this.props.joinedTable.player_id + 1}.</p>
            <div id="Seating">
              <p>{this.showPlayer(0)}</p>
              <p>{this.showPlayer(1)}</p>
              <p>{this.showPlayer(2)}</p>
              <p>{this.showPlayer(3)}</p>
              <div><button onClick={this.leaveTable}>Zurück</button></div>
            </div>
          </div>);
      }
    }
  }
}

export default Table;